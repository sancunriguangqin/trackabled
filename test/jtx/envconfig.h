//------------------------------------------------------------------------------
/*
    This file is part of trackabled: https://github.com/trackable/trackabled
    Copyright (c) 2012-2017 Trackable Labs Inc.

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

#ifndef TRACKABLE_TEST_JTX_ENVCONFIG_H_INCLUDED
#define TRACKABLE_TEST_JTX_ENVCONFIG_H_INCLUDED

#include <trackable/core/Config.h>

namespace trackable {
namespace test {

/// @brief initializes a config object for use with jtx::Env
///
/// @param config the configuration object to be initialized
extern
void
setupConfigForUnitTests (Config& config);

namespace jtx {

/// @brief creates and initializes a default
/// configuration for jtx::Env
///
/// @return unique_ptr to Config instance
inline
std::unique_ptr<Config>
envconfig()
{
    auto p = std::make_unique<Config>();
    setupConfigForUnitTests(*p);
    return p;
}

/// @brief creates and initializes a default configuration for jtx::Env and
/// invokes the provided function/lambda with the configuration object.
///
/// @param modfunc callable function or lambda to modify the default config.
/// The first argument to the function must be std::unique_ptr to
/// trackable::Config. The function takes ownership of the unique_ptr and
/// relinquishes ownership by returning a unique_ptr.
///
/// @param args additional arguments that will be passed to
/// the config modifier function (optional)
///
/// @return unique_ptr to Config instance
template <class F, class... Args>
std::unique_ptr<Config>
envconfig(F&& modfunc, Args&&... args)
{
    return modfunc(envconfig(), std::forward<Args>(args)...);
}

/// @brief adjust config so no admin ports are enabled
///
/// this is intended for use with envconfig, as in
/// envconfig(no_admin)
///
/// @param cfg config instance to be modified
///
/// @return unique_ptr to Config instance
std::unique_ptr<Config>
no_admin(std::unique_ptr<Config>);

/// @brief adjust configuration with params needed to be a validator
///
/// this is intended for use with envconfig, as in
/// envconfig(validator, myseed)
///
/// @param cfg config instance to be modified
/// @param seed seed string for use in secret key generation. A fixed default
/// value will be used if this string is empty
///
/// @return unique_ptr to Config instance
std::unique_ptr<Config>
validator(std::unique_ptr<Config>, std::string const&);

/// @brief adjust the default configured server ports by a specified value
///
/// This is intended for use with envconfig, as in
/// envconfig(port_increment, 5)
///
/// @param cfg config instance to be modified
/// @param int amount by which to increment the existing server port
/// values in the config
///
/// @return unique_ptr to Config instance
std::unique_ptr<Config>
port_increment(std::unique_ptr<Config>, int);

} // jtx
} // test
} // trackable

#endif

