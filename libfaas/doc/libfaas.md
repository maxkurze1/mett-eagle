# Libfaas

This package provides a [binary](../python2.7/src/wrapper.cc) (`python-faas2.7`)
for executing serverless python functions as well as a
[library](../lib/src/wrapper.cc) (`libfaas`) with c++ bindings that can be used
to for developing binary serverless functions.

## libfaas library

A very basic example function might look like this:

```cpp
#include <l4/libfaas/faas>

/**
 * @brief Do nothing
 * This is a function implementation that immediately returns
 */
std::string
Main (std::string /* args */)
{
  return "Hello from function";
}
```

### faas header utilities

The `faas` header also provides utility methods. These can for example be used
to call another function.

```cpp
#include <l4/libfaas/faas>

std::string
Main (std::string /* args */)
{
  // ... other code
  std::string ans = L4Re::Faas::invoke (
    "function1", // <-- name as declared by the client
    "hello fn1"  // <-- argument
    );
  // ... other code
}
```

This `invoke` function blocks until an answer is received.

For more information, read the comments in the [header](../include/faas).

### linking

To build these binaries, the Makefile should include:

```make
REQUIRES_LIBS += libfaas
```

## python-faas2.7 binary

This binary will be used automatically by the server in case a python function
is provided. It contains a python 2.7 runtime and executes a dataspace
containing python code.

A python serverless function might look like this:

```python
def main(arg):
  print "Hello from python: ", arg, "!"
  return "im done"
```

Additionally, the python binary provides a module called `faas` which contains
python wrappers for the server-worker interface.

These can be used like this:

```python
import faas

def main(arg):
  ret = faas.action_invoke(name="function2", arg="hey from fn1");
  return ret;
```

For more information about available methods and their calling scheme, have a look at [py-faas-lib.cc](../python2.7/src/py-faas-lib.cc).
