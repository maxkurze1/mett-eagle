# Libfmt

This is a port of the {fmt} [library](https://github.com/fmtlib/fmt). This
should be the same library as the c++20 std::format.

The library headers are accessible under `<l4/fmt/...>`.

## Usage

To use the fmt library:

- include the `<l4/fmt/core.h>` header
- add `REQUIRES_LIBS += libfmt` to the Makefile that build the executable
- add `requires: libfmt` to your package 'Control' file

Here is an example code snippet using the format library:

```cpp
#include <string>
#include <l4/fmt/core.h>

std::string s = fmt::format("{} {}", "Hello", "World");
```

## Logging

The format library is especially useful when used in conjunction with the
`liblog`. It is used internally for formatting the log message.

```cpp
#include <l4/liblog/log>
using namespace L4Re::LibLog;

int x = 9;
log<ERROR>("The value of x is {}", x);
```

As a consequence `liblog` supports logging of every class which provides a formatter implementation.
For example, this formatter is provided by `liblog` itself to provide convenient error logging:

```cpp
/**
 * @brief Custom formatter for L4::Base_exception
 * 
 * @see https://fmt.dev/latest/api.html#formatting-user-defined-types
 */
template <> struct fmt::formatter<L4::Base_exception>
{
  constexpr auto
  parse (format_parse_context &ctx) -> format_parse_context::iterator
  {
    return ctx.end ();
  }
  auto
  format (const L4::Base_exception &exc, format_context &ctx) const
      -> format_context::iterator
  {
    return fmt::format_to (ctx.out (), "{:s}", exc.str ());
  }
};
```

This formatter will be used if a `Base_exception` is logged. This may look like this:

```cpp
try {
  // .. some code throwing Base_exceptions
} catch (L4::Base_exception &e) {
  // just print the exception
  log<FATAL> (e);
  // or using the exception inside a custom error message
  log<FATAL> ("Some base exc occurred: {}", e);
}
```