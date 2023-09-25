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

const int COUNT = 1'000'000; // 100M ~ 4seconds

// double sq_arr[COUNT];
volatile double sq_arr;

std::string
Main (std::string args)
{
  auto start = std::chrono::high_resolution_clock::now ();

  // blocked waiting
  // std::this_thread::sleep_for (std::chrono::milliseconds (std::stoi(args)));
  

  // computing stuff
  for (int i = 0; i < COUNT; i++)
    sq_arr = (double)i * (double)i;

  
  // busy waiting
  // long time = std::stoi(args);
  // while (std::chrono::high_resolution_clock::now () - start < std::chrono::milliseconds(time));

  auto end = std::chrono::high_resolution_clock::now ();


  auto duration
      = std::chrono::duration_cast<std::chrono::microseconds> (end - start);

  return fmt::format ("{}", duration.count ());
}