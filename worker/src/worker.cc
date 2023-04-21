#include <stdio.h>

int
main (int argc, char *argv[])
{
  puts ("Hello from worker! args:");
  for (int i = 0; i < argc; i++)
    {
      puts (argv[i]);
    }
}