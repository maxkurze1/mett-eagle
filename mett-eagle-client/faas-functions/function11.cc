#include <l4/libfaas/faas>

/**
 * @brief Only wait for specific time
 *
 * This implementation is a simple function that waits for a specific amount of
 * time and returns it's own measured runtime.
 *
 * @param args the time to wait for in milliseconds
 * @return std::string The measured runtime as a string
 */

const int COUNT = 10000000;

double sq_arr[COUNT];

std::string
Main (std::string args)
{
  auto start = std::chrono::high_resolution_clock::now ();


  for (int i = 0; i < COUNT; i++)
    sq_arr[i] = (double)i * (double)i;

  // std::this_thread::sleep_for (std::chrono::milliseconds (std::stoi(args)));

  auto end = std::chrono::high_resolution_clock::now ();


  auto duration
      = std::chrono::duration_cast<std::chrono::microseconds> (end - start);

  return fmt::format ("{}", duration.count ());
}