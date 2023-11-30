/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
/**
 * @file
 * This file contains the C++ to python translations for the faas interface.
 *
 * This interface can be accessed using
 * @code{.py}
 * import faas
 * @endcode
 *
 * @see <l4/libfaas/faas>
 */
#include "py-faas-lib.h"

#include <l4/libfaas/faas>

#include <l4/sys/compiler.h>

/**
 * @brief Action invocation function
 *
 * This method can be used to invoke another serverless function of the same
 * client recursively.
 * 
 * Example:
 * @code{.py}
 * import faas
 * 
 * def main(arg):
 *   ret = faas.action_invoke(name="function2", arg="hey from fn1");
 *   return ret;
 * @endcode
 */
static PyObject *
faas_action_invoke (PyObject *self, PyObject *args, PyObject *keywds)
{
  /* this variable will hold the parsed argument  */
  const char *name;
  const char *arg;

  static char *kwlist[] = { "name", "arg", NULL };
  if (L4_UNLIKELY (!PyArg_ParseTupleAndKeywords (args, keywds, "ss", kwlist,
                                                 &name, &arg)))
    return NULL;

  std::string ret = L4Re::Faas::invoke (name, arg);

  /* return the string as python string */
  return Py_BuildValue ("s", ret.c_str ());
}

PyMethodDef faas_methods[]
    = { { "action_invoke", (PyCFunction)faas_action_invoke,
          METH_VARARGS | METH_KEYWORDS, "Invoke another serverless function" },
        { NULL, NULL, 0, NULL } };
