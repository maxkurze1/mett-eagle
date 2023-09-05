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
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/scheduler>
#include <l4/sys/thread>

#include <bitset>
#include <memory>
#include <pthread-l4.h>
#include <pthread.h>
#include <strings.h> // needed for ffsll

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

long
Manager_Registry_Epiface::op_register_client (
    MettEagle::Manager_Registry::Rights,
    L4::Ipc::Cap<MettEagle::Manager_Client> &manager_ipc_gate)
{
  log<DEBUG> ("Registering client");

  /**
   * This function will create a new thread (just like pthreads) that is
   * responsible for handling this particular client.
   */

#if 0
    /**
     * To have a more fine grained control over the thread, no library (e.g.
     * pthreads) will be used.
     *
     * The thread will get an own Scheduler to ensure that it will run on a
     * specific core.
     */
    auto env = const_cast<L4Re::Env *> (L4Re::Env::env ());
    // get utcb for thread

    // l4_utcb_tcr_u(u)->free_marker; ?? == 0 => usable

    // allocate utcb space
    l4_addr_t kernel_user_mem = 0;
    chksys (env->rm ()->reserve_area (&kernel_user_mem, L4_PAGESIZE,
                                      L4Re::Rm::F::Reserved
                                          | L4Re::Rm::F::Search_addr),
            "reserve area for utcb");

    chksys (env->task ()->add_ku_mem (
                l4_fpage (kernel_user_mem, L4_PAGESHIFT, L4_FPAGE_RW)),
            "get kernel user mem", [&] (l4_msgtag_t const &) {
              env->rm ()->free_area (kernel_user_mem);
            });

    auto client_thread = L4Re::Util::cap_alloc.alloc<L4::Thread> ();
    chksys (env->factory ()->create (client_thread), "create client thread");

    L4::Thread::Attr attr;
    attr.pager (env->rm ());
    attr.exc_handler (env->rm ());
    attr.bind ((l4_utcb_t *)env->first_free_utcb (), env->task ());

    env->first_free_utcb (env->first_free_utcb () + L4_UTCB_OFFSET);

    int test = 4;
    // L4Re::LibLog::chksys (client_thread->control (attr), "setup app
    // thread %d", test, [](const l4_msgtag_t &tag) -> void {
    //   log<DEBUG>("called callback");
    // });

    auto stack_cap = chkcap (
        L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> (), "alloc cap");

    // alloc single page for the thread stack
    int stack_size = 4096;
    chksys (env->mem_alloc ()->alloc (stack_size, stack_cap));

    l4_addr_t stack_addr;
    chksys (
        env->rm ()->attach (&stack_addr, stack_size,
                            L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                            L4::Ipc::make_cap_rw (stack_cap), L4_STACK_ALIGN),
        "attaching stack vma");

    chksys (
        client_thread->ex_regs (
            (l4_addr_t)client_function /* some address */,
            l4_align_stack_for_direct_fncall ((unsigned long)stack_addr), 0),
        "start app thread");

    // run thread
    log<DEBUG> ("running test thread");

    chksys (env->scheduler ()->run_thread (
        client_thread, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));
#endif

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

        /**
         * TODO end loop and exit thread after client disconnect to free
         * resources again
         *
         * pthread_cancel - probably not
         * pthread_exit ?? - should be the answer
         *
         * is it possible to get notified if passed ipc_gate was deleted
         */
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

  pthread_attr_destroy (&attr);

  client_server
      = new L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> (
          thread_cap, L4Re::Env::env ()->factory ());
  /* create new object handling the requests of this client */
  /* TODO handle deallocation */
  auto epiface = new Manager_Client_Epiface (thread_cap, sched_cap);

  /* register the object in the server loop. This will create the        *
   * capability for the object and inform the server to route IPC there. */
  L4::Cap<void> cap = client_server->registry ()->register_obj (epiface);
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

  chksys (sched_cap->run_thread (
      thread_cap, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));

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