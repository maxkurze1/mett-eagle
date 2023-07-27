/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/libfaas/faas>
#include <l4/liblog/log>
#include <l4/liblog/loggable-exception>

#include <l4/mett-eagle/alias>

#include <l4/re/env>
#include <l4/re/error_helper>

#include <string>

#include <l4/sys/utcb.h>

#include <l4/fmt/core.h>

using L4Re::LibLog::Log;
using L4Re::LibLog::Loggable_exception;

/**
 * @brief Wrapper main that will handle the manager interaction
 */
int
main (int argc, const char *argv[])
try
  {
    /* there has to be exactly one argument -- the string passed */
    if (L4_UNLIKELY (argc != 1))
      throw Loggable_exception (
          -L4_EINVAL, fmt::format("Wrong number of arguments. Expected 1 got {:d}", argc));

    /* actual call to the faas function */
    std::string ret{ Main (argv[0]) };

    /* the default _exit implementation can only return an integer *
     * to pass a string the custom manager->exit must be used.     */
    L4Re::chksys (L4Re::Faas::getManager ()->exit (ret.c_str ()), "exit rpc");

    __builtin_unreachable();
  }
/**
 * These catch blocks will catch errors that are thrown by utility
 * methods like L4Re::chkcap or by the faas function itself
 */
catch (L4Re::LibLog::Loggable_exception &e)
  {
    Log::fatal (fmt::format("{}",e));
    return e.err_no (); // propagate the error to the client
  }
catch (L4::Runtime_error &e)
  {
    Log::fatal (fmt::format("{}",e));
    return e.err_no (); // propagate the error to the client
  }
catch (... /* catch all */)
  {
    Log::fatal ("Function threw unknown error.");
    return -L4_EINVAL;
  }