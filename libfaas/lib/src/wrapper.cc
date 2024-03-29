/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include <l4/libfaas/faas>
#include <l4/liblog/log>
#include <l4/liblog/loggable-exception>

#include <l4/re/env>
#include <l4/re/error_helper>

#include <string>

#include <l4/sys/utcb.h>

#include <l4/fmt/core.h>

using namespace L4Re::LibLog;
using L4Re::LibLog::Loggable_exception;

#include <l4/sys/kdebug.h>

/* timing data of the worker */
L4Re::MettEagle::Worker_Metadata metadata;

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
          -L4_EINVAL, "Wrong number of arguments. Expected 1 got {:d}", argc);

    /* actual call to the faas function */
    metadata.start_function = std::chrono::high_resolution_clock::now ();
    metadata.start_runtime = metadata.start_function;
    std::string ret{ Main (argv[0]) };
    metadata.end_function = std::chrono::high_resolution_clock::now ();
    metadata.end_runtime = metadata.end_function;

    /* the default _exit implementation can only return an integer *
     * to pass a string the custom manager->exit must be used.     */
    L4Re::chksys (L4Re::Faas::getManager ()->exit (ret.c_str (), metadata), "exit rpc");

    throw Loggable_exception(-L4_EFAULT, "wrapper unreachable");
  }
/**
 * These catch blocks will catch errors that are thrown by utility
 * methods like L4Re::chkcap or by the faas function itself
 */
catch (L4Re::LibLog::Loggable_exception &e)
  {
    log<FATAL> (e);
    return e.err_no (); // propagate the error to the client
  }
catch (L4::Runtime_error &e)
  {
    log<FATAL> (e);
    return e.err_no (); // propagate the error to the client
  }
catch (... /* catch all */)
  {
    log<FATAL> ("Function threw unknown error.");
    return -0xDEAD; // -57005 (keep for grepping)
  }