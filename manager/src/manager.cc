#include "app_model.h"
#include "app_task.h"
#include "debug.h"
#include "exec.h"
#include "server.h"

#include <l4/re/l4aux.h>
#include <l4/cxx/iostream>
#include <l4/mett-eagle/mett-eagle>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <uclibc/stdlib.h>

/**
 * what the hell does the debug mask do???
 */
Dbg dbg (Dbg::Server, "Manager");

/**
 * A server that processes can talk to ??
 */
// static Server s;
// Server *server = &s;

/**
 * Absolutely no clue what this does..
 * Something with the kip ...
 * KIP === Kernel Interface Page
 */
l4re_aux_t *l4re_aux;

/**
 * this is the server the client will talk to ??
 * this is the server everything will talk to ....
 * 
 * Map interfaces to the object implementing them??
 */
L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

/**
 * Implementation of the MettEagle interface
 */
class Calculation_server : public L4::Epiface_t<Calculation_server, MettEagle>
{
public:
  long
  op_invoke (MettEagle::Rights, L4::Ipc::String<> name, l4_uint32_t &res)
  {
    printf ("Got invoke: %s\n", name.data);
    char name_str[name.length];
    memcpy(name_str, name.data, sizeof(char) * name.length);
    execve(name.data);
    res = 5;

    return EXIT_SUCCESS;
  }
};

/**
 * This is the entry point of the Mett-Eagle manager
 * server. This method will be started by ned, as specified
 * in the mett-eagle.cfg
 */
int
main (const int argc, const char *const argv[])
try
  {
    l4_umword_t *auxp = (l4_umword_t *)&argv[argc] + 1;
    while (*auxp)
      ++auxp;
    ++auxp;

    l4re_aux = 0;

    while (*auxp)
      {
        if (*auxp == 0xf0)
          l4re_aux = (l4re_aux_t *)auxp[1];
        auxp += 2;
      }

    const char *const args[] = { "Hello", "world", "!", NULL };
    const char *const envp[] = { "test=somevalue", NULL };
    const Cap log = { L4Re::Env::env ()->log (), 16, 16, "cap_name" };
    const Cap log2 = { L4Re::Env::env ()->log (), 16, 16, "other_name" };
    const Cap *const capabilities[] = { &log, &log2, NULL };

    // execve ("rom/worker", args, envp, capabilities);
    // execve ("rom/worker", NULL, NULL, NULL);
    // execve ("rom/worker", NULL);

    // client interaction =====================

    static Calculation_server calc;

    // Register calculation server
    L4Re::chkcap (server.registry ()->register_obj (&calc, "server"),
                  "Could not register my service, is there a 'server' in "
                  "the caps table?\n");

    printf ("Started Mett-Eagle server!\n");

    // Wait for client requests
    server.loop ();
  }
catch (L4::Runtime_error &e)
  {
    L4::cerr << "FATAL: " << e;
    return 1;
  }