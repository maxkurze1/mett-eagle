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


// L4Re::Util::Object_registry registry{ &server_interface };

int
main (const int _argc, const char *const _argv[])
try
  {
    (void)_argc;
    (void)_argv;

    /* the capability used to register this client at the server */
    // L4::Cap<MettEagle::ManagerRegistry> manager_registry = L4Re::chkcap (
    //     L4Re::Env::env ()->get_cap<MettEagle::ManagerRegistry> ("manager"),
    //     "Couldn't get manager registry capability");

    // /**
    //  * Send ipc gate to the mett-eagle server and get one back.
    //  */
    // L4::Cap<MettEagle::Manager> manager
    //     = L4Re::chkcap (L4Re::Util::cap_alloc.alloc<MettEagle::Manager> (),
    //                     "allocate manager capability");

    // L4Re::chkcap (reg.registry ()->register_obj (&client_epiface),
    //               "Couldn't register IPC gate");

    // log_debug ("Registered IPC gate %d",
    //            client_epiface.obj_cap ().is_valid ());
    // L4Re::chksys (manager_registry->register_client (client_epiface.obj_cap (), manager),
    //               "register_client");
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
    return EXIT_FAILURE;
  }