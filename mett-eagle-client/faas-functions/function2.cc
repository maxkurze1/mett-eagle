#include <l4/libfaas/faas>
#include <l4/liblog/log>

/**
 * @brief Recursive function call tester
 *
 * This function will invoke a recursive function
 * call, calling function1
 *
 * Therefore it is necessary that function1 has already
 * been defined
 */

using namespace L4Re::LibLog;

std::string
Main (std::string /* args */)
{
  // log<INFO> ("Hello from function2, param: '{}'", args);

  std::string ans = L4Re::Faas::invoke ("function1", "50"); // 50 milliseconds

  // log<INFO> ("Invoked function1: '{}'", ans);

  return "function2 long name";
}