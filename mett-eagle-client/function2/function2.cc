#include <l4/liblog/log>
#include <l4/libfaas/faas>

std::string Main(std::string args) {
  log_info("Hello from function2, param: %s", args.c_str());

  return "Hello from function2";
}