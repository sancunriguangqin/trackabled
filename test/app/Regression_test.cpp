//------------------------------------------------------------------------------
/*
    This file is part of trackabled: https://github.com/trackable/trackabled
    Copyright (c) 2012, 2013 Trackable Labs Inc.
    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.
    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <BeastConfig.h>
#include <test/jtx.h>
#include <test/jtx/envconfig.h>
#include <trackable/app/tx/apply.h>
#include <trackable/basics/StringUtilities.h>
#include <trackable/json/json_reader.h>
#include <trackable/protocol/Feature.h>
#include <trackable/protocol/JsonFields.h>

namespace trackable {
namespace test {

struct Regression_test : public beast::unit_test::suite
{
    // OfferCreate, then OfferCreate with cancel
    void testOffer1()
    {
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gw");
        auto const USD = gw["USD"];
        env.fund(XRP(10000), "alice", gw);
        env(offer("alice", USD(10), XRP(10)), require(owners("alice", 1)));
        env(offer("alice", USD(20), XRP(10)), json(R"raw(
                { "OfferSequence" : 2 }
            )raw"), require(owners("alice", 1)));
    }

    void testLowBalanceDestroy()
    {
        testcase("Account balance < fee destroys correct amount of XRP");
        using namespace jtx;
        Env env(*this);
        env.memoize("alice");

        // The low balance scenario can not deterministically
        // be reproduced against an open ledger. Make a local
        // closed ledger and work with it directly.
        auto closed = std::make_shared<Ledger>(
            create_genesis, env.app().config(),
            std::vector<uint256>{}, env.app().family());
        auto expectedDrops = SYSTEM_CURRENCY_START;
        BEAST_EXPECT(closed->info().drops == expectedDrops);

        auto const aliceXRP = 400;
        auto const aliceAmount = XRP(aliceXRP);

        auto next = std::make_shared<Ledger>(
            *closed,
            env.app().timeKeeper().closeTime());
        {
            // Fund alice
            auto const jt = env.jt(pay(env.master, "alice", aliceAmount));
            OpenView accum(&*next);

            auto const result = trackable::apply(env.app(),
                accum, *jt.stx, tapNONE, env.journal);
            BEAST_EXPECT(result.first == tesSUCCESS);
            BEAST_EXPECT(result.second);

            accum.apply(*next);
        }
        expectedDrops -= next->fees().base;
        BEAST_EXPECT(next->info().drops == expectedDrops);
        {
            auto const sle = next->read(
                keylet::account(Account("alice").id()));
            BEAST_EXPECT(sle);
            auto balance = sle->getFieldAmount(sfBalance);

            BEAST_EXPECT(balance == aliceAmount );
        }

        {
            // Specify the seq manually since the env's open ledger
            // doesn't know about this account.
            auto const jt = env.jt(noop("alice"), fee(expectedDrops),
                seq(1));

            OpenView accum(&*next);

            auto const result = trackable::apply(env.app(),
                accum, *jt.stx, tapNONE, env.journal);
            BEAST_EXPECT(result.first == tecINSUFF_FEE);
            BEAST_EXPECT(result.second);

            accum.apply(*next);
        }
        {
            auto const sle = next->read(
                keylet::account(Account("alice").id()));
            BEAST_EXPECT(sle);
            auto balance = sle->getFieldAmount(sfBalance);

            BEAST_EXPECT(balance == XRP(0));
        }
        expectedDrops -= aliceXRP * dropsPerXRP<int>::value;
        BEAST_EXPECT(next->info().drops == expectedDrops);
    }

    void testSecp256r1key ()
    {
        testcase("Signing with a secp256r1 key should fail gracefully");
        using namespace jtx;
        Env env(*this);

        // Test case we'll use.
        auto test256r1key = [&env] (Account const& acct)
        {
            auto const baseFee = env.current()->fees().base;
            std::uint32_t const acctSeq = env.seq (acct);
            Json::Value jsonNoop = env.json (
                noop (acct), fee(baseFee), seq(acctSeq), sig(acct));
            JTx jt = env.jt (jsonNoop);
            jt.fill_sig = false;

            // Random secp256r1 public key generated by
            // https://kjur.github.io/jsrsasign/sample-ecdsa.html
            std::string const secp256r1PubKey =
                "045d02995ec24988d9a2ae06a3733aa35ba0741e87527"
                "ed12909b60bd458052c944b24cbf5893c3e5be321774e"
                "5082e11c034b765861d0effbde87423f8476bb2c";

            // Set the key in the JSON.
            jt.jv["SigningPubKey"] = secp256r1PubKey;

            // Set the same key in the STTx.
            auto secp256r1Sig = std::make_unique<STTx>(*(jt.stx));
            auto pubKeyBlob = strUnHex (secp256r1PubKey);
            assert (pubKeyBlob.second); // Hex for public key must be valid
            secp256r1Sig->setFieldVL
                (sfSigningPubKey, std::move(pubKeyBlob.first));
            jt.stx.reset (secp256r1Sig.release());

            env (jt, ter (temINVALID));
        };

        Account const alice {"alice", KeyType::secp256k1};
        Account const becky {"becky", KeyType::ed25519};

        env.fund(XRP(10000), alice, becky);

        test256r1key (alice);
        test256r1key (becky);
    }

    void testFeeEscalationAutofill()
    {
        testcase("Autofilled fee should use the escalated fee");
        using namespace jtx;
        Env env(*this, envconfig([](std::unique_ptr<Config> cfg)
            {
                cfg->section("transaction_queue")
                    .set("minimum_txn_in_ledger_standalone", "3");
                return cfg;
            }),
            with_features(featureFeeEscalation));
        Env_ss envs(env);

        auto const alice = Account("alice");
        env.fund(XRP(100000), alice);

        auto params = Json::Value(Json::objectValue);
        // Max fee = 50k drops
        params[jss::fee_mult_max] = 5000;
        std::vector<int> const
            expectedFees({ 10, 10, 8889, 13889, 20000 });

        // We should be able to submit 5 transactions within
        // our fee limit.
        for (int i = 0; i < 5; ++i)
        {
            envs(noop(alice), fee(none), seq(none))(params);

            auto tx = env.tx();
            if (BEAST_EXPECT(tx))
            {
                BEAST_EXPECT(tx->getAccountID(sfAccount) == alice.id());
                BEAST_EXPECT(tx->getTxnType() == ttACCOUNT_SET);
                auto const fee = tx->getFieldAmount(sfFee);
                BEAST_EXPECT(fee == drops(expectedFees[i]));
            }
        }
    }

    void testJsonInvalid()
    {
        using namespace jtx;
        using boost::asio::buffer;
        testcase("jsonInvalid");

        std::string const request = R"json({"command":"path_find","id":19,"subcommand":"create","source_account":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","destination_account":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","destination_amount":"1000000","source_currencies":[{"currency":"0000000000000000000000000000000000000000"},{"currency":"0000000000000000000000005553440000000000"},{"currency":"0000000000000000000000004254430000000000"},{"issuer":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","currency":"0000000000000000000000004254430000000000"},{"issuer":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","currency":"0000000000000000000000004254430000000000"},{"issuer":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","currency":"0000000000000000000000004555520000000000"},{"currency":"0000000000000000000000004554480000000000"},{"currency":"0000000000000000000000004A50590000000000"},{"issuer":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","currency":"000000000000000000000000434E590000000000"},{"currency":"0000000000000000000000004742490000000000"},{"issuer":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh","currency":"0000000000000000000000004341440000000000"}]})json";

        Json::Value jvRequest;
        Json::Reader jrReader;

        std::vector<boost::asio::const_buffer> buffers;
        buffers.emplace_back(buffer(request, 1024));
        buffers.emplace_back(buffer(request.data() + 1024, request.length() - 1024));
        BEAST_EXPECT(jrReader.parse(jvRequest, buffers) &&
            jvRequest && jvRequest.isObject());
    }

    void run() override
    {
        testOffer1();
        testLowBalanceDestroy();
        testSecp256r1key();
        testFeeEscalationAutofill();
        testJsonInvalid();
    }
};

BEAST_DEFINE_TESTSUITE(Regression,app,trackable);

} // test
} // trackable
