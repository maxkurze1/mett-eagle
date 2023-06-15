#include <l4/liblog/log>
#include <l4/libfaas/util>

std::string Main(std::string args) {
  log_info("Hello from function1, param: %s", args.c_str());

  return "Hello from function1";
}