#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/env_ns>
#include <l4/sys/err.h>
#include <l4/sys/types.h>

#include <l4/cxx/iostream>
#include <l4/liblog/log>
#include <l4/mett-eagle/client>
#include <l4/mett-eagle/util>
#include <l4/mett-eagle/manager>
#include <l4/re/util/br_manager>
#include <l4/sys/cxx/ipc_server_loop>
#include <l4/sys/ipc_gate>
#include <stdio.h>

#include <time.h>
#include <unistd.h>


// L4Re::Util::Object_registry registry{ &server_interface };

int
main (const int _argc, const char *const _argv[])
try
  {
    (void)_argc;
    (void)_argv;

    auto manager = MettEagle::getManager("manager");

    log_debug ("Register done");

    auto file = MettEagle::open_file("rom/function1");

    L4Re::chksys (manager->action_create ("test", file),
                  "action_create");

    MettEagle::invoke("test");

    log_info ("function returned");

    return EXIT_SUCCESS;
  }
catch (L4::Runtime_error &e)
  {
    log_fatal (e);
    return e.err_no();
  }