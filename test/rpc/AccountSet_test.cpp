//------------------------------------------------------------------------------
/*
    This file is part of trackabled: https://github.com/trackable/trackabled
    Copyright (c) 2016 Trackable Labs Inc.

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

#include <trackable/basics/StringUtilities.h>
#include <trackable/protocol/AmountConversions.h>
#include <trackable/protocol/Feature.h>
#include <trackable/protocol/JsonFields.h>
#include <trackable/protocol/Quality.h>
#include <trackable/protocol/Rate.h>
#include <test/jtx.h>

namespace trackable {

class AccountSet_test : public beast::unit_test::suite
{
public:

    void testNullAccountSet()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), notrackable(alice));
        //ask for the ledger entry - account root, to check its flags
        auto const jrr = env.le(alice);
        BEAST_EXPECT((*env.le(alice))[ sfFlags ] == 0u);
    }

    void testSetAndReset(unsigned int flag_val, std::string const& label)
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), notrackable(alice));
        env.memoize("eric");
        env(regkey(alice, "eric"));

        unsigned int orig_flags = (*env.le(alice))[ sfFlags ];

        env.require(nflags(alice, flag_val));
        env(fset(alice, flag_val), sig(alice));
        env.require(flags(alice, flag_val));
        env(fclear(alice, flag_val));
        env.require(nflags(alice, flag_val));
        uint32 now_flags = (*env.le(alice))[ sfFlags ];
        BEAST_EXPECT(now_flags == orig_flags);
    }

    void testSetAndResetAccountTxnID()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), notrackable(alice));

        unsigned int orig_flags = (*env.le(alice))[ sfFlags ];

        // asfAccountTxnID is special and not actually set as a flag,
        // so we check the field presence instead
        BEAST_EXPECT(! env.le(alice)->isFieldPresent(sfAccountTxnID));
        env(fset(alice, asfAccountTxnID), sig(alice));
        BEAST_EXPECT(env.le(alice)->isFieldPresent(sfAccountTxnID));
        env(fclear(alice, asfAccountTxnID));
        BEAST_EXPECT(! env.le(alice)->isFieldPresent(sfAccountTxnID));
        uint32 now_flags = (*env.le(alice))[ sfFlags ];
        BEAST_EXPECT(now_flags == orig_flags);
    }

    void testSetNoFreeze()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), notrackable(alice));
        env.memoize("eric");
        env(regkey(alice, "eric"));

        env.require(nflags(alice, asfNoFreeze));
        env(fset(alice, asfNoFreeze), sig("eric"), ter(tecNEED_MASTER_KEY));
        env(fset(alice, asfNoFreeze), sig(alice));
        env.require(flags(alice, asfNoFreeze));
        env(fclear(alice, asfNoFreeze), sig(alice));
        // verify flag is still set (clear does not clear in this case)
        env.require(flags(alice, asfNoFreeze));
    }

    void testDomain()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), alice);
        auto jt = noop(alice);
        // The Domain field is represented as the hex string of the lowercase
        // ASCII of the domain. For example, the domain example.com would be
        // represented as "6578616d706c652e636f6d".
        //
        // To remove the Domain field from an account, send an AccountSet with
        // the Domain set to an empty string.
        std::string const domain = "example.com";
        jt[sfDomain.fieldName] = strHex(domain);
        env(jt);
        BEAST_EXPECT((*env.le(alice))[ sfDomain ] == makeSlice(domain));

        jt[sfDomain.fieldName] = "";
        env(jt);
        BEAST_EXPECT(! env.le(alice)->isFieldPresent(sfDomain));

        // The upper limit on the length is 256 bytes
        // (defined as DOMAIN_BYTES_MAX in SetAccount)
        // test the edge cases: 255, 256, 257.
        std::size_t const maxLength = 256;
        for (std::size_t len = maxLength - 1; len <= maxLength + 1; ++len)
        {
            std::string domain2 =
                std::string(len - domain.length() - 1, 'a') + "." + domain;

            BEAST_EXPECT (domain2.length() == len);

            jt[sfDomain.fieldName] = strHex(domain2);

            if (len <= maxLength)
            {
                env(jt);
                BEAST_EXPECT((*env.le(alice))[ sfDomain ] == makeSlice(domain2));
            }
            else
            {
                env(jt, ter(telBAD_DOMAIN));
            }
         }
    }

    void testMessageKey()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), alice);
        auto jt = noop(alice);

        auto const rkp = randomKeyPair(KeyType::ed25519);
        jt[sfMessageKey.fieldName] = strHex(rkp.first.slice());
        env(jt);
        BEAST_EXPECT(strHex((*env.le(alice))[ sfMessageKey ]) == strHex(rkp.first.slice()));

        jt[sfMessageKey.fieldName] = "";
        env(jt);
        BEAST_EXPECT(! env.le(alice)->isFieldPresent(sfMessageKey));

        jt[sfMessageKey.fieldName] = strHex("NOT_REALLY_A_PUBKEY");
        env(jt, ter(telBAD_PUBLIC_KEY));
    }

    void testWalletID()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), alice);
        auto jt = noop(alice);

        uint256 somehash = from_hex_text<uint256>("9633ec8af54f16b5286db1d7b519ef49eefc050c0c8ac4384f1d88acd1bfdf05");
        jt[sfWalletLocator.fieldName] = to_string(somehash);
        env(jt);
        BEAST_EXPECT((*env.le(alice))[ sfWalletLocator ] == somehash);

        jt[sfWalletLocator.fieldName] = "";
        env(jt);
        BEAST_EXPECT(! env.le(alice)->isFieldPresent(sfWalletLocator));
    }

    void testEmailHash()
    {
        using namespace test::jtx;
        Env env(*this);
        Account const alice ("alice");
        env.fund(XRP(10000), alice);
        auto jt = noop(alice);

        uint128 somehash = from_hex_text<uint128>("fff680681c2f5e6095324e2e08838f221a72ab4f");
        jt[sfEmailHash.fieldName] = to_string(somehash);
        env(jt);
        BEAST_EXPECT((*env.le(alice))[ sfEmailHash ] == somehash);

        jt[sfEmailHash.fieldName] = "";
        env(jt);
        BEAST_EXPECT(! env.le(alice)->isFieldPresent(sfEmailHash));
    }

    void testTransferRate()
    {
        struct test_results
        {
            double set;
            TER code;
            double get;
        };

        using namespace test::jtx;
        auto doTests = [this] (FeatureBitset const& features,
            std::initializer_list<test_results> testData)
        {
            Env env (*this, features);

            Account const alice ("alice");
            env.fund(XRP(10000), alice);

            for (auto const& r : testData)
            {
                env(rate(alice, r.set), ter(r.code));

                // If the field is not present expect the default value
                if (!(*env.le(alice))[~sfTransferRate])
                    BEAST_EXPECT(r.get == 1.0);
                else
                    BEAST_EXPECT(*(*env.le(alice))[~sfTransferRate] ==
                        r.get * QUALITY_ONE);
            }
        };

        {
            testcase ("Setting transfer rate (without fix1201)");
            doTests (all_features_except(fix1201),
                {
                    { 1.0, tesSUCCESS,              1.0 },
                    { 1.1, tesSUCCESS,              1.1 },
                    { 2.0, tesSUCCESS,              2.0 },
                    { 2.1, tesSUCCESS,              2.1 },
                    { 0.0, tesSUCCESS,              1.0 },
                    { 2.0, tesSUCCESS,              2.0 },
                    { 0.9, temBAD_TRANSFER_RATE,    2.0 }});
        }

        {
            testcase ("Setting transfer rate (with fix1201)");
            doTests (all_amendments(),
                {
                    { 1.0, tesSUCCESS,              1.0 },
                    { 1.1, tesSUCCESS,              1.1 },
                    { 2.0, tesSUCCESS,              2.0 },
                    { 2.1, temBAD_TRANSFER_RATE,    2.0 },
                    { 0.0, tesSUCCESS,              1.0 },
                    { 2.0, tesSUCCESS,              2.0 },
                    { 0.9, temBAD_TRANSFER_RATE,    2.0 }});
        }
    }

    void testGateway()
    {
        using namespace test::jtx;
        auto runTest = [](Env&& env, double tr)
        {
            Account const alice ("alice");
            Account const bob ("bob");
            Account const gw ("gateway");
            auto const USD = gw["USD"];

            env.fund(XRP(10000), gw, alice, bob);
            env.trust(USD(3), alice, bob);
            env(rate(gw, tr));
            env.close();

            auto const amount = USD(1);
            Rate const rate (tr * QUALITY_ONE);
            auto const amountWithRate =
                toAmount<STAmount> (multiply(amount.value(), rate));

            env(pay(gw, alice, USD(3)));
            env(pay(alice, bob, USD(1)), sendmax(USD(3)));

            env.require(balance(alice, USD(3) - amountWithRate));
            env.require(balance(bob, USD(1)));
        };

        // Test gateway with allowed transfer rates
        runTest (Env{*this, all_features_except(fix1201)}, 1.02);
        runTest (Env{*this, all_features_except(fix1201)}, 1);
        runTest (Env{*this, all_features_except(fix1201)}, 2);
        runTest (Env{*this, all_features_except(fix1201)}, 2.1);
        runTest (Env{*this, all_amendments()}, 1.02);
        runTest (Env{*this, all_amendments()}, 2);

        // Test gateway when amendment is set after transfer rate
        {
            Env env (*this, all_features_except(fix1201));
            Account const alice ("alice");
            Account const bob ("bob");
            Account const gw ("gateway");
            auto const USD = gw["USD"];
            double const tr = 2.75;

            env.fund(XRP(10000), gw, alice, bob);
            env.trust(USD(3), alice, bob);
            env(rate(gw, tr));
            env.close();
            env.enableFeature(fix1201);
            env.close();

            auto const amount = USD(1);
            Rate const rate (tr * QUALITY_ONE);
            auto const amountWithRate =
                toAmount<STAmount> (multiply(amount.value(), rate));

            env(pay(gw, alice, USD(3)));
            env(pay(alice, bob, amount), sendmax(USD(3)));

            env.require(balance(alice, USD(3) - amountWithRate));
            env.require(balance(bob, amount));
        }
    }

    void testBadInputs(bool withFeatures)
    {
        using namespace test::jtx;
        std::unique_ptr<Env> penv {
            withFeatures ?  new Env(*this) : new Env(*this, no_features)};
        Env& env = *penv;
        Account const alice ("alice");
        env.fund(XRP(10000), alice);

        auto jt = fset(alice, asfDisallowXRP);
        jt[jss::ClearFlag] = asfDisallowXRP;
        env(jt, ter(temINVALID_FLAG));

        jt = fset(alice, asfRequireAuth);
        jt[jss::ClearFlag] = asfRequireAuth;
        env(jt, ter(temINVALID_FLAG));

        jt = fset(alice, asfRequireDest);
        jt[jss::ClearFlag] = asfRequireDest;
        env(jt, ter(temINVALID_FLAG));

        jt = fset(alice, asfDisallowXRP);
        jt[sfFlags.fieldName] = tfAllowXRP;
        env(jt, ter(temINVALID_FLAG));

        jt = fset(alice, asfRequireAuth);
        jt[sfFlags.fieldName] = tfOptionalAuth;
        env(jt, ter(temINVALID_FLAG));

        jt = fset(alice, asfRequireDest);
        jt[sfFlags.fieldName] = tfOptionalDestTag;
        env(jt, ter(temINVALID_FLAG));

        jt = fset(alice, asfRequireDest);
        jt[sfFlags.fieldName] = tfAccountSetMask;
        env(jt, ter(temINVALID_FLAG));

        env(fset (alice, asfDisableMaster),
            sig(alice),
            ter(withFeatures ? tecNO_ALTERNATIVE_KEY : tecNO_REGULAR_KEY));
    }

    void testRequireAuthWithDir()
    {
        using namespace test::jtx;
        Env env(*this, with_features(featureMultiSign));
        Account const alice ("alice");
        Account const bob ("bob");

        env.fund(XRP(10000), alice);
        env.close();

        // alice should have an empty directory.
        BEAST_EXPECT(dirIsEmpty (*env.closed(), keylet::ownerDir(alice)));

        // Give alice a signer list, then there will be stuff in the directory.
        env(signers(alice, 1, { { bob, 1} }));
        env.close();
        BEAST_EXPECT(! dirIsEmpty (*env.closed(), keylet::ownerDir(alice)));

        env(fset (alice, asfRequireAuth), ter(tecOWNERS));
    }

    void run()
    {
        testNullAccountSet();
        for(auto const& flag_set : std::vector<std::pair<unsigned int, std::string>>({
            {asfRequireDest,   "RequireDestTag"},
            {asfRequireAuth,   "RequireAuth"},
            {asfDisallowXRP,   "DisallowXRP"},
            {asfGlobalFreeze,  "GlobalFreeze"},
            {asfDisableMaster, "DisableMaster"},
            {asfDefaultTrackable, "DefaultTrackable"}
        }))
        {
            testSetAndReset(flag_set.first, flag_set.second);
        }
        testSetAndResetAccountTxnID();
        testSetNoFreeze();
        testDomain();
        testGateway();
        testMessageKey();
        testWalletID();
        testEmailHash();
        testBadInputs(true);
        testBadInputs(false);
        testRequireAuthWithDir();
        testTransferRate();
    }


};

BEAST_DEFINE_TESTSUITE(AccountSet,app,trackable);

}

