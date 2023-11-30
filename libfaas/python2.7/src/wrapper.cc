/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
#include "py-faas-lib.h"

#include <l4/python/Python.h>

#include <l4/libfaas/faas>
#include <l4/liblog/log>
#include <l4/liblog/loggable-exception>

#include <l4/mett-eagle/worker>
#include <l4/re/env>
#include <l4/re/error_helper>

#include <string>

#include <cstdio>
#include <l4/sys/utcb.h>

#include <l4/fmt/core.h>

using namespace L4Re::LibLog;
using L4Re::LibLog::Loggable_exception;

/* timing data of the worker */
L4Re::MettEagle::Worker_Metadata metadata;

static const char *
invoke_python_main (const char *filename, std::string arg)
{
  Py_NoSiteFlag = 1; /* do not try to import 'site' */
  Py_Initialize ();
  /* make the faas library available inside python  */
  Py_InitModule ("faas", faas_methods);

  auto file = fopen (filename, "r");

  auto pModule = PyImport_AddModule ("__main__");

  /* this interprets the whole script -- all methods are defined, global
   * variables are created and global code is executes (main method is not
   * executed at this point!) */
  // todo maybe this should also be measured as 'function time' rather than
  // adding to the runtime setup?
  PyRun_SimpleFileEx (file, filename, true);

  if (L4_UNLIKELY (pModule == NULL))
    throw Loggable_exception (-L4_EINVAL,
                              "Could not create '__main__' module");

  /* get the main method from the script */
  auto pFunc = PyObject_GetAttrString (pModule, "main");

  if (L4_UNLIKELY (pFunc == NULL))
    throw Loggable_exception (-L4_EINVAL, "Could not find 'main' method");
  if (L4_UNLIKELY (not PyCallable_Check (pFunc)))
    throw Loggable_exception (-L4_EINVAL, "'main' is not callable");

  /* arguments are a tuple -- only 1 string will be passed */
  auto pArgs = PyTuple_New (1);
  auto pValue = PyString_FromString (arg.c_str ());
  if (L4_UNLIKELY (pValue == NULL))
    throw Loggable_exception (
        -L4_EINVAL, "Failed to convert std::string to python string");
  PyTuple_SetItem (pArgs, 0, pValue);

  /* perform the actual (main) function call */
  metadata.start_function = std::chrono::high_resolution_clock::now ();
  pValue = PyObject_CallObject (pFunc, pArgs);
  metadata.end_function = std::chrono::high_resolution_clock::now ();

  Py_DECREF (pArgs); /* arguments are no longer needed */

  /* if function returned nothing use an empty string */
  const char *ret;
  if (pValue == NULL)
    ret = "";
  else
    ret = PyString_AsString (pValue);

  /* values no longer needed */
  Py_XDECREF (pValue); /* XDECREF -> value may be NULL */
  Py_DECREF (pFunc);
  Py_DECREF (pModule);
  // Py_Finalize (); todo fix 'No signal handler found' error

  return ret;
}

/**
 * @brief Wrapper main that will handle the manager interaction
 */
int
main (int argc, const char *argv[])
try
  {
    (void)argv;

    /* there has to be exactly one argument -- the string passed */
    if (L4_UNLIKELY (argc != 1))
      throw Loggable_exception (
          -L4_EINVAL, "Wrong number of arguments. Expected 1 got {:d}", argc);

    /* This wrapper expects an initial dataspace 'function' which will be
     * opened as file and executed as python script */
    L4Re::chkcap (L4Re::Env::env ()->get_cap<L4Re::Dataspace> ("function"),
                  "no capability called 'function' passed");

    metadata.start_runtime = std::chrono::high_resolution_clock::now ();

    /* actual call to the faas function */
    auto answer = invoke_python_main ("function", argv[0]);

    metadata.end_runtime = std::chrono::high_resolution_clock::now ();

    /* the default _exit implementation can only return an integer *
     * to pass a string the custom manager->exit must be used.     */
    L4Re::chksys (L4Re::Faas::getManager ()->exit (answer, metadata),
                  "exit rpc");

    __builtin_unreachable ();
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
    return -L4_EINVAL;
  }