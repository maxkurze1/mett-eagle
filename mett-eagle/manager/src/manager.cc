/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "manager.h"
#include "manager_registry.h"

#include <l4/liblog/exc_log_dispatch>

#include <l4/re/util/object_registry>

using namespace L4Re::LibLog;


/**
 * @see manager.h
 */
std::bitset<sizeof (l4_sched_cpu_set_t::map) * 8> available_cpus;

/**
 * This object will handle the registration of clients.
 */
L4Re::Util::Registry_server<> register_server;

#include <l4/sys/debugger.h>

/**
 * This is the entry point of the Mett-Eagle manager
 * server. This method will be started by ned, as specified
 * in the mett-eagle.cfg
 */
int
main (const int /* argc */, const char *const /* argv */[])
try
  {
    l4_debugger_set_object_name(L4Re::Env::env()->task().cap(), "mngr");
    l4_debugger_set_object_name(L4Re::Env::env()->main_thread().cap(), "mngr reg");

    /**
     * Query available cpu-set which can be distributed to clients
     */
    l4_umword_t cpu_max;
    l4_sched_cpu_set_t cpus{ l4_sched_cpu_set (0, 0) };
    chksys (L4Re::Env::env ()->scheduler ()->info (&cpu_max, &cpus),
                  "failed to query scheduler info");

    /* reserve last cpu for all clients and manager registry thread */
    cpus.map &= ~1LL;

    /* update global bitmap */
    available_cpus = cpus.map;

    log<INFO> ("Scheduler info (available cpus) :: {:0{}b} => {:d}/{:d}",
               cpus.map, cpu_max, available_cpus.count (), cpu_max);

    /*
     * Associate the 'server' endpoint that was already
     * 'reserved' by ned with a newly created interface
     * implementation
     */
    chkcap (register_server.registry ()->register_obj (
                      new Manager_Registry_Epiface (), "server"),
                  "Couldn't register service, is there a 'server' in "
                  "the caps table?");

    log<INFO> ("Starting Mett-Eagle registry server!");

    // start server loop -- loop will not return!
    // started with custom dispatch to log errors
    register_server.internal_loop (
        Exc_log_dispatch<L4Re::Util::Object_registry &> (
            *register_server.registry ()),
        l4_utcb ());
  }
/**
 * Catch all errors (e.g. from chkcap) and log some message
 */
catch (L4::Runtime_error &e)
  {
    log<FATAL> (e);
    return e.err_no ();
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    log<FATAL> (e);
    return e.err_no ();
  }