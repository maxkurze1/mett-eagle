/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "manager_registry.h"
#include "manager.h"
#include "manager_client.h"

#include <l4/liblog/exc_log_dispatch>

#include <l4/re/env>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/scheduler>
#include <l4/sys/thread>

#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <pthread-l4.h>
#include <pthread.h>
#include <strings.h> // needed for ffsll

#include <l4/sys/debugger.h>

/**
 * This will select a cpu for a newly connected client
 *
 * It uses the global available_cpus.map and returns the first set bit starting
 * from the least significant bit.
 *
 * @return l4_umword_t  Cpu bitmap with 1 selected cpu
 */
static l4_umword_t
select_client_cpu ()
{
  if (L4_UNLIKELY (available_cpus.none ()))
    throw Loggable_exception (-L4_ENOENT, "No cpu available");
  auto selected = 1ULL << (ffsll (available_cpus.to_ullong ()) - 1);
  available_cpus &= ~selected; /* mark cpu as unavailable */
  return selected;
}

/**
 * Mark previously used CPUs as free, after client disconnection.
 *
 * @arg free_bitmap  Bitmap with 1's for every cpu to mark as free
 */
static void
free_client_cpu (const l4_umword_t free_bitmap)
{
  available_cpus |= free_bitmap;
}

/**
 * This is an irq implementation that will be invoked if an ipc_gate connected
 * to the client handler thread is deleted.
 */
class Gate_deletion_irq : public L4::Irqep_t<Gate_deletion_irq>
{
private:
  /* this gate is the one that's handed out to the client and checked if it was
   * deleted by the client */
  L4::Cap<L4::Ipc_gate> _client_gate;
  /* custom object cleanup method, called before pthread_exit */
  std::function<void (void)> _cleanup;

public:
  Gate_deletion_irq (
      L4::Cap<L4::Ipc_gate> client_gate,
      std::function<void (void)> cleanup = [] {})
      : _client_gate (client_gate), _cleanup (cleanup)
  {
  }

  void
  handle_irq ()
  {
    /* check if the deleted gate is the one of the client, and not one of a
     * worker process */
    if (L4_LIKELY (!_client_gate.validate ().label () == 0))
      return;

    log<DEBUG> ("deleting client thread");

    _cleanup ();

    delete this;

    /* the handler will be called by the thread that should be deleted  *
     * thus it's possible to use pthread_exit instead of pthread_cancel */

    // TODO somehow the l4::thread-obj is not deleted??
    pthread_exit (nullptr);

    throw Loggable_exception (-L4_EFAULT, "Pthread exit failed");
  }
};

long
Manager_Registry_Epiface::op_register_client (
    MettEagle::Manager_Registry::Rights,
    L4::Ipc::Cap<MettEagle::Manager_Client> &manager_ipc_gate)
{
  log<DEBUG> ("Registering client");

  /** scheduler with only one selected cpu enabled for the new thread */
  auto sched_cap = L4Re::Util::make_shared_cap<L4::Scheduler> ();
  l4_mword_t limit = L4_SCHED_MAX_PRIO;
  l4_mword_t offset = L4_SCHED_MIN_PRIO;

  /**
   * "Bitmap" with only one bit set. So each thread will run on it's own cpu.
   * The same 'Scheduler' will be used for the started processes of the
   * client.
   */
  l4_umword_t bitmap = select_client_cpu ();
  log<DEBUG> ("Selected cpu {:#b}", bitmap);

  chksys (
      l4_msgtag_t (L4Re::Env::env ()->user_factory ()->create<L4::Scheduler> (
                       sched_cap.get ())
                   << limit << offset << bitmap),
      "Failed to create scheduler");
  l4_debugger_set_object_name (sched_cap.cap (), "mngr clnt shed");

  pthread_t pthread;
  pthread_attr_t attr;

  pthread_attr_init (&attr);
  /*
   * This will prevent pthreads from directly starting the new thread and is
   * necessary to make sure that the passed pointer argument points to a valid
   * object.
   */
  attr.create_flags |= PTHREAD_L4_ATTR_NO_START;

  /*
   * Br_manager is necessary to handle the demand of cap slots. They
   * are used e.g. to receive the Dataspace of a client while creating
   * a new action.
   *
   * double pointer will be use to ensure that the pointer value stays valid
   * after this function ends
   *
   * here only the pointer will be allocated *not* the object itself
   */
  auto client_server_pointer
      = new L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> *();
  auto &client_server = *(client_server_pointer);
  /* create thread without starting */
  int failed = pthread_create (
      &pthread, &attr,
      [] (void *_arg) -> void * {
        auto arg
            = (L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> **)
                _arg;
        /* copy reference after object creation */
        std::unique_ptr<
            L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> >
            client_server{ *arg };
        /* free the allocated pointer again - do *not* free the object */
        delete arg;

        log<INFO> ("Start client ipc server");

        /* loop and whole client will be terminated by deletion irq */
        client_server->internal_loop (
            Exc_log_dispatch<L4Re::Util::Object_registry &> (
                *client_server->registry ()),
            l4_utcb ());
      },
      client_server_pointer);
  if (failed)
    {
      // TODO probably better throw
      log<ERROR> ("failed to create thread");
    }
  auto thread_cap = L4::Cap<L4::Thread> (pthread_l4_cap (pthread));
  l4_debugger_set_object_name (thread_cap.cap (), "mngr clnt");

  pthread_attr_destroy (&attr);

  client_server
      = new L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> (
          thread_cap, L4Re::Env::env ()->factory ());
  /* create new object handling the requests of this client */
  auto epiface = new Manager_Client_Epiface (thread_cap, sched_cap);

  /* register the object in the server loop. This will create the        *
   * capability for the object and inform the server to route IPC there. */
  L4::Cap<void> cap = client_server->registry ()->register_obj (epiface);
  l4_debugger_set_object_name (cap.cap (), "clnt->mngr");
  if (L4_UNLIKELY (not cap.is_valid ()))
    {
      client_server->registry ()->unregister_obj (epiface);
      delete client_server;
      delete client_server_pointer;
      // TODO destroy thread -- cancel?
      auto canceled = pthread_cancel (pthread) == 0;
      throw Loggable_exception (
          -L4_ENOMEM, "Failed to register client IPC gate, thread_canceled={}",
          canceled);
    }

  /*
   * The in kernel ref-count of the ipc_gate will be decreased in order to get
   * notified if the client deletes its pointer to the gate
   */
  chksys (epiface->obj_cap ()->dec_refcnt (1), "dec_refcnt of client epiface");
  /* register IRQ on gate_deletion */
  auto deletion_irq
      = new Gate_deletion_irq (L4::cap_cast<L4::Ipc_gate> (cap), [=] {
          /* this will be called when the client deletes his gate*/
          delete epiface;
          delete client_server;

          /* free resources */
          free_client_cpu (bitmap);
        });
  chkcap (client_server->registry ()->register_irq_obj (deletion_irq),
          "gate deletion irq");
  thread_cap->register_del_irq (deletion_irq->obj_cap ());

  chksys (sched_cap->run_thread (thread_cap,
                                 l4_sched_param (L4RE_MAIN_THREAD_PRIO)));

  /*
   * Note: The thread might start the server loop after the ipc call returned
   * to the client. In case the client sends a request before the server loop
   * is started it will be blocked by the ipc until the thread calls
   * server::loop() and thereby start an open wait.
   */

  /* Pass the IPC gate back to the client as output argument. */
  manager_ipc_gate = epiface->obj_cap ();
  /* separate thread from the 'client_handler' object */
  pthread_detach (pthread);
  return L4_EOK;
}