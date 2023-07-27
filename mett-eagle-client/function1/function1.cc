#include <l4/liblog/log>
#include <l4/libfaas/faas>

#include <cstdlib>
#include <cmath>
#include <list>
#include <l4/re/env>

using L4Re::LibLog::Log;

constexpr unsigned long chunk_size = 8192UL;

std::list<void *> mem;

std::string Main(std::string args) {
  Log::info(fmt::format("Hello from function1, param: {}", args));
  // std::string ans = L4Re::Faas::invoke("test_idk");
  
  
  // int size = 0;
  // while (true) {
  //   void *some_mem = malloc(chunk_size);
  //   memset(some_mem, 0xFF, chunk_size);
  //   mem.push_back(some_mem); // keep reference
  //   size += chunk_size;
  //   Log::info(fmt::format("Malloced {}", size));
  // }
  return "function1 idk "; // + ans;
}