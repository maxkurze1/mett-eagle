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
#include <l4/mett-eagle/alias>
#include <l4/re/util/br_manager>
#include <l4/sys/cxx/ipc_server_loop>
#include <l4/sys/ipc_gate>
#include <stdio.h>

#include <time.h>
#include <unistd.h>
#include <thread>

int
main (const int _argc, const char *const _argv[])
try
  {
    (void)_argc;
    (void)_argv;

    log_debug("creating thread");

    

    // auto manager = MettEagle::getManager("manager");

    log_debug ("Register done");

    log_debug("join done");

    // manager->action_create ("test", "rom/function1");
    // std::string ans = manager->action_invoke ("test");

    // log_info ("function returned %s\n", ans.c_str());

    return EXIT_SUCCESS;
  }
catch (L4::Runtime_error &e)
  {
    log_fatal (e);
    return e.err_no();
  }