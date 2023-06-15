# Liblog the logging library

## Description

The purpose of this library is to make the log look more consistent and to make
it easier for application developers.

The library provides the header 'l4/liblog/log' which contains 5 utility
functions for logging (with ascending priority: debug, info, warn, error and
fatal). These functions will accept printf-style arguments and will print the
message with the corresponding log level.

NOTE: The log functions will print a line break at the end per default!

This should make sure that log messages aren't lost inside buffers. Also there
is no use in printing multiple invocations to the same line since they will all
be prefixed with the log information.

Which messages are printed is defined by the 'output log level'. This level can
be set using the setLevel function. The default Log level is INFO which means
that info-messages and all higher priority messages will be printed.

## Usage

To use this library the 'Control'-file of the project should contain
`requires: liblog`. (I think) this will guarantee that the projects are compiled
in the correct order. To also link the program binary with this lib there should
be `REQUIRES_LIBS += liblog` inside the Makefile building the binary.

## Configuration

The library can be configured through environment variables.

### LOG_LEVEL

This variable should contain one of 5 values (Debug, Info, Warn, Error or
Fatal). The case will be ignored. If not set, the level will default to INFO.

### PKG_NAME

If this variable is set the name will be printed in the log prefix. This can be
used to identify different processes in the log.

## Example

To log a message its only necessary to include the header and call one of the 5
utility methods

```cpp
#include <l4/liblog/log>

int some_var = 5;

log_debug("Debug %p", &some_var);
log_info("This is some info %s with a number: %d", "message", some_var);
log_warn("There's something missing");
log_error("That should not happen");
log_fatal("I can't recover from here");
```

Also L4 errors can be directly passed to the log.

It will print the Base_exception#str.

```cpp
try
  {
  // anything ...
  }
catch (L4::Base_exception &exc)
  {
    log_fatal(exc);
  }
```

For the L4::Runtime_error, Runtime_error#str and Runtime_error#str_extra will be
printed.

```cpp
try
  {
   // anything ...
  }
catch (L4::Runtime_error &exc)
  {
    log_fatal(exc);
  }
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
    LOG_LEVEL = "warn"
})
```