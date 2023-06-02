#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/sys/err.h>
#include <l4/sys/types.h>

#include <l4/cxx/iostream>
#include <l4/mett-eagle/mett-eagle>
#include <stdio.h>

int
main ()
try
  {
    L4::Cap<MettEagle> server
        = L4Re::chkcap (L4Re::Env::env ()->get_cap<MettEagle> ("manager"),
                        "Couldn't get manager capability", 0);

    printf ("Invoking mett-eagle server\n");

    l4_uint32_t res;

    L4Re::chksys(server->invoke ("rom/worker", &res), "Error talking to server");

    printf ("function returned with res: %d\n", res);

    // l4_uint32_t val1 = 8;
    // l4_uint32_t val2 = 5;

    // printf ("Asking for %d - %d\n", val1, val2);
    // if (server->sub (val1, val2, &val1))
    //   {
    //     printf ("Error talking to server\n");
    //     return 1;
    //   }
    // printf ("Result of subtract call: %d\n", val1);

    // printf ("Asking for -%d\n", val1);
    // if (server->neg (val1, &val1))
    //   {
    //     printf ("Error talking to server\n");
    //     return 1;
    //   }
    // printf ("Result of negate call: %d\n", val1);

    return 0;
  }
catch (L4::Runtime_error &e)
  {
    L4::cerr << "FATAL: " << e;
    return 1;
  }