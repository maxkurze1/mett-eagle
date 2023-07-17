#include <l4/liblog/log>
#include <l4/libfaas/faas>

using L4Re::LibLog::Log;

std::string Main(std::string args) {
  Log::info("Hello from function2, param: %s", args.c_str(),"filename");

  return "function2 long name";
}