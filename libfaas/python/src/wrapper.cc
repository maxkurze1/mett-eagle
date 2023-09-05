/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
#define PY_SSIZE_T_CLEAN
#include <l4/python3/Python.h>

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

static void
invoke_python_main ()
{
  PyObject *pModule, *pFunc;
  PyObject *pArgs, *pValue;

  PyConfig config;

  PyConfig_InitPythonConfig (&config);
  config.use_hash_seed = 1;
  config.hash_seed = 0;
  config.platlibdir = (wchar_t *)L"/rom";
  // config.site_import = 0;

  Py_InitializeFromConfig (&config);
  // pName = PyString_FromString ("rom/test.py");
  /* Error checking of pName left out */
  log<DEBUG> ("Py_Init done");

  pModule = PyImport_AddModule ("__main__");

  log<DEBUG> ("Added module");

  PyRun_SimpleString ("def test_function(str):\n  return \"some answer\"");
  PyRun_SimpleString ("import sys;"
                      "print(\"hello world\", file=sys.stderr)");

  log<DEBUG> ("Ran string");

  if (pModule != NULL)
    {
      pFunc = PyObject_GetAttrString (pModule, "test_function");
      /* pFunc is a new reference */

      if (pFunc && PyCallable_Check (pFunc))
        {
          pArgs = PyTuple_New (1);
          pValue = PyUnicode_FromString ("argument string");
          /* pValue reference stolen here: */
          PyTuple_SetItem (pArgs, 0, pValue);
          pValue = PyObject_CallObject (pFunc, pArgs);
          Py_DECREF (pArgs);
          if (pValue != NULL)
            {
              Py_ssize_t size;
              auto string = PyUnicode_AsUTF8AndSize (pValue, &size);
              printf ("Result of call: %.*s\n", (int)size, string);
              Py_XDECREF (string);
              Py_DECREF (pValue);
            }
          else
            {
              Py_DECREF (pFunc);
              Py_DECREF (pModule);
              PyErr_Print ();
              fprintf (stderr, "Call failed\n");
              return;
            }
        }
      else
        {
          if (PyErr_Occurred ())
            PyErr_Print ();
          fprintf (stderr, "Cannot find function\n");
        }
      Py_XDECREF (pFunc);
      Py_DECREF (pModule);
    }
  else
    {
      PyErr_Print ();
      fprintf (stderr, "Failed to load \"%s\"\n", "filename");
      return;
    }
  Py_Finalize ();
  return;
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
          -L4_EINVAL, "Wrong number of arguments. Expected 1 got {:d}",
                       argc);

    log<DEBUG> ("Trying to invoke python");
    /* actual call to the faas function */
    invoke_python_main ();

    /* the default _exit implementation can only return an integer *
     * to pass a string the custom manager->exit must be used.     */
    L4Re::chksys (L4Re::Faas::getManager ()->exit ("some return string"),
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