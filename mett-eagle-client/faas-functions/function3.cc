#include <l4/libfaas/faas>
#include <l4/liblog/log>

#include <cmath>
#include <cstdlib>
#include <l4/re/env>
#include <list>

/**
 * @brief Memory limit tester
 *
 * This function will allocate memory in a loop until 
 * it reaches the allocation limit.
 */

using namespace L4Re::LibLog;

constexpr unsigned long chunk_size = 8192UL;

std::list<void *> mem;

std::string
Main (std::string /* args */)
{
  int size = 0;
  while (true)
    {
      void *some_mem = malloc (chunk_size);
      // make sure to touch memory to prevent COW
      memset (some_mem, 0xFF, chunk_size);
      mem.push_back (some_mem); // keep reference
      size += chunk_size;
      
      // log<INFO> ("Malloced {}", size);
    }

  return "";
}