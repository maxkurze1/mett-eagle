#include <l4/libfaas/faas>
#include <l4/liblog/log>

#include <cmath>
#include <cstdlib>
#include <l4/re/env>
#include <list>

using namespace L4Re::LibLog;

constexpr unsigned long chunk_size = 8192UL;

std::list<void *> mem;

std::string
Main (std::string args)
{
  auto start = std::chrono::high_resolution_clock::now ();

  log<INFO> ("Hello from function1, param: {}", args);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  // std::string ans = L4Re::Faas::invoke("test_idk");

  // int size = 0;
  // while (true) {
  //   void *some_mem = malloc(chunk_size);
  //   memset(some_mem, 0xFF, chunk_size);
  //   mem.push_back(some_mem); // keep reference
  //   size += chunk_size;
  //   log<INFO>("Malloced {}", size);
  // }
  auto end = std::chrono::high_resolution_clock::now ();

  auto duration
      = std::chrono::duration_cast<std::chrono::microseconds> (end - start);

  return fmt::format("{}", duration.count()); // + ans;
}