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
#include <test/jtx/sig.h>
#include <test/jtx/utility.h>

namespace trackable {
namespace test {
namespace jtx {

void
sig::operator()(Env&, JTx& jt) const
{
    if (! manual_)
        return;
    jt.fill_sig = false;
    if(account_)
    {
        // VFALCO Inefficient pre-C++14
        auto const account = *account_;
        jt.signer = [account](Env&, JTx& jt)
        {
            jtx::sign(jt.jv, account);
        };
    }
}

} // jtx
} // test
} // trackable
