#include <l4/liblog/log>
#include <l4/libfaas/faas>

using L4Re::LibLog::Log;

std::string Main(std::string args) {
  Log::info(fmt::format("Hello from function2, param: '{}'", args));

  return "function2 long name";
}