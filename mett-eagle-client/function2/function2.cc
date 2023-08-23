#include <l4/liblog/log>
#include <l4/libfaas/faas>

using namespace L4Re::LibLog;

std::string Main(std::string args) {
  log<INFO>("Hello from function2, param: '{}'", args);

  return "function2 long name";
}