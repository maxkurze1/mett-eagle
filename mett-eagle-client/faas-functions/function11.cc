#include <l4/libfaas/faas>

/**
 * @brief Busy Loop
 *
 * This implementation is a simple function that starts a busy loop for a
 * specific amount of iterations and returns it's own measured runtime.
 *
 * @param args the amount of iterations (as string)
 * @return std::string The measured runtime in microseconds (as string)
 */

std::string
Main (std::string args)
{
  auto start = std::chrono::high_resolution_clock::now ();

  auto iterations = std::stoi (args);
  double sum = 0.0;
  for (int i = 0; i < iterations; i++)
    sum += (double)i * (double)i;

  auto end = std::chrono::high_resolution_clock::now ();

  auto duration
      = std::chrono::duration_cast<std::chrono::microseconds> (end - start);

  return fmt::format ("{}", duration.count ());
}