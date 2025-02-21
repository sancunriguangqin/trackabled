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

#ifndef TRACKABLE_TEST_WSCLIENT_H_INCLUDED
#define TRACKABLE_TEST_WSCLIENT_H_INCLUDED

#include <test/jtx/AbstractClient.h>
#include <trackable/core/Config.h>
#include <boost/optional.hpp>
#include <chrono>
#include <memory>

namespace trackable {
namespace test {

class WSClient : public AbstractClient
{
public:
    /** Retrieve a message. */
    virtual
    boost::optional<Json::Value>
    getMsg(std::chrono::milliseconds const& timeout =
        std::chrono::milliseconds{0}) = 0;

    /** Retrieve a message that meets the predicate criteria. */
    virtual
    boost::optional<Json::Value>
    findMsg(std::chrono::milliseconds const& timeout,
        std::function<bool(Json::Value const&)> pred) = 0;
};

/** Returns a client operating through WebSockets/S. */
std::unique_ptr<WSClient>
makeWSClient(Config const& cfg, bool v2 = true, unsigned rpc_version = 2);

} // test
} // trackable

#endif
