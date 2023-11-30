# Liblog the logging library

## Description

The purpose of this library is to make the log look more consistent and logging
more convenient application developers.

This library provides the `<l4/liblog/log>` header which contains utility
functions for logging. These functions will accept `{fmt}`-style arguments.

NOTE: The log functions will print a line break at the end per default!

Which messages are printed is defined by the 'output log level'. This level can
be set using the `setLevel` function. The default Log level is `INFO` which
means that info-messages and all higher priority messages will be printed.

## Usage

You just need to include the corresponding header. Nothing else needs to be
done.

## Configuration

The library can be configured through environment variables.

### LOG_LEVEL

This variable should contain a bitmap of the enabled Levels. If not set, the
level will default to `INFO` (= `0b01111`). This bitmap should have 5 bits
representing debug, info, warn, error, and fatal in exactly this order. The
`setLevel` function also sets this environment variable.

### PKGNAME

If this variable is set the name will be printed in the log prefix. This can be
used to identify different processes in the log.

## Examples

To log a message its only necessary to include the header and call one of the 5
utility methods

```cpp
#include <l4/liblog/log>

using namespace L4Re::LibLog;

int some_var = 5;

log<DEBUG>("Debug {:p}", &some_var);
log<INFO> ("This is some info {:s} with a number: {:s}", "message", some_var);
log<WARN> ("There's something missing");
log<ERROR>("That should not happen");
log<FATAL>("I can't recover from here");
```

An example configuration (using ned) would look like this:

```lua
local L4 = require("L4");

L4.default_loader:start({
    caps = {
        ...
    },
    log = L4.Env.log, -- start without log color or prefix from ned
}, "rom/binary-name",
{
    -- Environment variables
    PKGNAME   = "Some package",
    LOG_LEVEL = 0x1F -- all levels active
})
```

In this configuration the L4.Env.log will be passed directly from ned. This
prevents ned from adding a prefix or a color to the log.

## Errors

Some L4 errors can be directly passed to the log.

```cpp
catch (L4::Base_exception &exc)
  {
    log<FATAL> (exc);
  }
```

For Base_exception's the Base_exception#str will be printed.

```cpp
catch (L4::Runtime_error &exc)
  {
    log<FATAL> (exc);
  }
```

For the L4::Runtime_error, Runtime_error#str and Runtime_error#str_extra will be
printed.

### Loggable Exception

There exists a special exception class that can be used to create an error that
contains information about the location that threw it. Another nice feature of
the loggable error is, that it can be constructed with a format string.

```cpp
#include <l4/liblog/log>
#include <l4/fmt/core.h>
#include <l4/liblog/loggable-exception>

using L4Re::LibLog::Loggable_exception;
using namespace L4Re::LibLog;

try
  {
    int bad_value = -1;
    if (bad_value < 0)
      throw Loggable_exception (-L4_EINVAL, "Value shouldn't be less than 0 (value={:d})", bad_value);
  }
catch (Loggable_exception &e)
  {
    log<FATAL> ("{}", e);
    return e.err_no ();
  }
```

## TODO

add chksys add chkcap add chkipc

with error lambda/function formatted output and Loggable_exception
