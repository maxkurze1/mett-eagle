#include <l4/liblog/log>
#include <l4/libfaas/faas>

std::string Main(std::string args) {
  log_info("Hello from function1, param: %s", args.c_str());
  std::string ans = L4Re::Faas::invoke("test_idk");
  return "function1 idk" + ans;
}