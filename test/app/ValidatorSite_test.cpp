//------------------------------------------------------------------------------
/*
    This file is part of trackabled: https://github.com/trackable/trackabled
    Copyright 2016 Trackable Labs Inc.

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

#include <beast/core/detail/base64.hpp>
#include <trackable/app/misc/ValidatorSite.h>
#include <trackable/basics/Slice.h>
#include <trackable/basics/strHex.h>
#include <trackable/protocol/digest.h>
#include <trackable/protocol/HashPrefix.h>
#include <trackable/protocol/PublicKey.h>
#include <trackable/protocol/SecretKey.h>
#include <trackable/protocol/Sign.h>
#include <test/jtx.h>
#include <test/jtx/TrustedPublisherServer.h>
#include <boost/asio.hpp>

namespace trackable {
namespace test {

class ValidatorSite_test : public beast::unit_test::suite
{
private:

    using Validator = TrustedPublisherServer::Validator;

    static
    PublicKey
    randomNode ()
    {
        return derivePublicKey (KeyType::secp256k1, randomSecretKey());
    }

    static
    std::string
    makeManifestString (
        PublicKey const& pk,
        SecretKey const& sk,
        PublicKey const& spk,
        SecretKey const& ssk,
        int seq)
    {
        STObject st(sfGeneric);
        st[sfSequence] = seq;
        st[sfPublicKey] = pk;
        st[sfSigningPubKey] = spk;

        sign(st, HashPrefix::manifest, *publicKeyType(spk), ssk);
        sign(st, HashPrefix::manifest, *publicKeyType(pk), sk,
            sfMasterSignature);

        Serializer s;
        st.add(s);

        return beast::detail::base64_encode (std::string(
            static_cast<char const*> (s.data()), s.size()));
    }

    static
    Validator
    randomValidator ()
    {
        auto const secret = randomSecretKey();
        auto const masterPublic =
            derivePublicKey(KeyType::ed25519, secret);
        auto const signingKeys = randomKeyPair(KeyType::secp256k1);
        return { masterPublic, signingKeys.first, makeManifestString (
            masterPublic, secret, signingKeys.first, signingKeys.second, 1) };
    }

    void
    testConfigLoad ()
    {
        testcase ("Config Load");

        using namespace jtx;

        Env env (*this);
        auto trustedSites = std::make_unique<ValidatorSite> (
            env.app().getIOService(), env.app().validators(), beast::Journal());

        // load should accept empty sites list
        std::vector<std::string> emptyCfgSites;
        BEAST_EXPECT(trustedSites->load (emptyCfgSites));

        // load should accept valid validator site uris
        std::vector<std::string> cfgSites({
            "http://trackable.com/",
            "http://trackable.com/validators",
            "http://trackable.com:8080/validators",
            "http://207.261.33.37/validators",
            "http://207.261.33.37:8080/validators",
            "https://trackable.com/validators",
            "https://trackable.com:443/validators"});
        BEAST_EXPECT(trustedSites->load (cfgSites));

        // load should reject validator site uris with invalid schemes
        std::vector<std::string> badSites(
            {"ftp://trackable.com/validators"});
        BEAST_EXPECT(!trustedSites->load (badSites));

        badSites[0] = "wss://trackable.com/validators";
        BEAST_EXPECT(!trustedSites->load (badSites));

        badSites[0] = "trackable.com/validators";
        BEAST_EXPECT(!trustedSites->load (badSites));
    }

    void
    testFetchList ()
    {
        testcase ("Fetch list");

        using namespace jtx;

        Env env (*this);
        auto& trustedKeys = env.app ().validators ();

        beast::Journal journal;

        PublicKey emptyLocalKey;
        std::vector<std::string> emptyCfgKeys;

        auto const publisherSecret1 = randomSecretKey();
        auto const publisherPublic1 =
            derivePublicKey(KeyType::ed25519, publisherSecret1);
        auto const pubSigningKeys1 = randomKeyPair(KeyType::secp256k1);

        auto const manifest1 = makeManifestString (
            publisherPublic1, publisherSecret1,
            pubSigningKeys1.first, pubSigningKeys1.second, 1);

        auto const publisherSecret2 = randomSecretKey();
        auto const publisherPublic2 =
            derivePublicKey(KeyType::ed25519, publisherSecret2);
        auto const pubSigningKeys2 = randomKeyPair(KeyType::secp256k1);

        auto const manifest2 = makeManifestString (
            publisherPublic2, publisherSecret2,
            pubSigningKeys2.first, pubSigningKeys2.second, 1);

        std::vector<std::string> cfgPublishers({
            strHex(publisherPublic1),
            strHex(publisherPublic2)});

        BEAST_EXPECT(trustedKeys.load (
            emptyLocalKey, emptyCfgKeys, cfgPublishers));

        auto constexpr listSize = 20;
        std::vector<Validator> list1;
        list1.reserve (listSize);
        while (list1.size () < listSize)
            list1.push_back (randomValidator());

        std::vector<Validator> list2;
        list2.reserve (listSize);
        while (list2.size () < listSize)
            list2.push_back (randomValidator());


        using endpoint_type = boost::asio::ip::tcp::endpoint;
        using address_type = boost::asio::ip::address;

        // Use ports of 0 to allow OS selection
        endpoint_type ep1{address_type::from_string("127.0.0.1"), 0};
        endpoint_type ep2{address_type::from_string("127.0.0.1"), 0};

        auto const sequence = 1;
        auto const version = 1;
        NetClock::time_point const expiration =
            env.timeKeeper().now() + 3600s;

        TrustedPublisherServer server1(
            ep1,
            env.app().getIOService(),
            pubSigningKeys1,
            manifest1,
            sequence,
            expiration,
            version,
            list1);

        TrustedPublisherServer server2(
            ep2,
            env.app().getIOService(),
            pubSigningKeys2,
            manifest2,
            sequence,
            expiration,
            version,
            list2);

        std::uint16_t const port1 = server1.local_endpoint().port();
        std::uint16_t const port2 = server2.local_endpoint().port();


        {
            // fetch single site
            std::vector<std::string> cfgSites(
            {"http://127.0.0.1:" + std::to_string(port1) + "/validators"});

            auto sites = std::make_unique<ValidatorSite> (
                env.app().getIOService(), env.app().validators(), journal);

            sites->load (cfgSites);
            sites->start();
            sites->join();

            for (auto const& val : list1)
            {
                BEAST_EXPECT(trustedKeys.listed (val.masterPublic));
                BEAST_EXPECT(trustedKeys.listed (val.signingPublic));
            }
        }
        {
            // fetch multiple sites
            std::vector<std::string> cfgSites({
            "http://127.0.0.1:" + std::to_string(port1) + "/validators",
            "http://127.0.0.1:" + std::to_string(port2) + "/validators"});

            auto sites = std::make_unique<ValidatorSite> (
                env.app().getIOService(), env.app().validators(), journal);

            sites->load (cfgSites);
            sites->start();
            sites->join();

            for (auto const& val : list1)
            {
                BEAST_EXPECT(trustedKeys.listed (val.masterPublic));
                BEAST_EXPECT(trustedKeys.listed (val.signingPublic));
            }

            for (auto const& val : list2)
            {
                BEAST_EXPECT(trustedKeys.listed (val.masterPublic));
                BEAST_EXPECT(trustedKeys.listed (val.signingPublic));
            }
        }
    }

public:
    void
    run() override
    {
        testConfigLoad ();
        testFetchList ();
    }
};

BEAST_DEFINE_TESTSUITE(ValidatorSite, app, trackable);

} // test
} // trackable
