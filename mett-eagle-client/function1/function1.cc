#include <l4/liblog/log>
#include <l4/libfaas/faas>

std::string Main(std::string args) {
  log_info("Hello from function1, param: %s", args.c_str());

  L4Re::Faas::invoke("test_idk");
  log_info("invoke returned");

  return "Hello from function1";
}