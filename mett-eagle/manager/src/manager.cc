/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "app_model.h"
#include "worker.h"

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <pthread-l4.h>
#include <pthread.h>
#include <string>
#include <thread>

#include <l4/fmt/core.h>
#include <l4/liblog/error_helper>
#include <l4/liblog/exc_log_dispatch>
#include <l4/liblog/log>
#include <l4/liblog/loggable-exception>
#include <l4/liballoc/alloc>
#include <l4/mett-eagle/manager>

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/l4aux.h>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/env_ns>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_string>
#include <l4/sys/thread>

#include <thread-l4>


namespace MettEagle = L4Re::MettEagle;
using L4Re::LibLog::chksys;
using L4Re::LibLog::Loggable_exception;

struct Manager_Base_Epiface : L4::Epiface_t0<MettEagle::Manager_Base>
{
protected:
  /**
   * @brief Per client namespace (map) for dataspace resolution
   *
   * This map will hold all registered actions of a client and also provide
   * them to the workers started by this specific client only.
   */
  std::shared_ptr<std::map<std::string, L4::Cap<L4Re::Dataspace> > > _actions;

  /**
   * @brief The client specific thread that will execute all actions
   */
  L4::Cap<L4::Thread> _thread;

  /**
   * @brief The Scheduler object that is used by the thread and worker
   * processes
   */
  L4::Cap<L4::Scheduler> _scheduler;

public:
  long op_action_invoke (MettEagle::Manager_Base::Rights,
                         const L4::Ipc::String_in_buf<> &name,
                         L4::Ipc::Array_ref<char> &ret);
};

struct Manager_Client_Epiface
    : L4::Epiface_t<Manager_Client_Epiface, MettEagle::Manager_Client,
                    Manager_Base_Epiface>
{
protected:
  l4_umword_t _cpu_bitmap;

public:
  Manager_Client_Epiface (L4::Cap<L4::Thread> thread,
                          L4::Cap<L4::Scheduler> scheduler,
                          l4_umword_t cpu_bitmap)
  {
    /* _actions map will be create by the clients epiface and only *
     * passed to each worker epiface.                              */
    _actions = std::make_shared<
        std::map<std::string, L4::Cap<L4Re::Dataspace> > > ();
    _thread = thread;
    _scheduler = scheduler;
    _cpu_bitmap = cpu_bitmap;
  }

  long
  op_action_create (MettEagle::Manager_Client::Rights,
                    const L4::Ipc::String_in_buf<> &name,
                    L4::Ipc::Snd_fpage file)
  {
    Log::debug (fmt::format ("Create action name='{:s}' file passed='{:d}' cap idx='{:#x}'",
                             name.data, file.cap_received (), server_iface ()->rcv_cap(0).cap()));
    if (L4_UNLIKELY (not file.cap_received ()))
      throw Loggable_exception (-L4_EINVAL, "No dataspace cap received");
    auto cap = server_iface ()->rcv_cap<L4Re::Dataspace> (0);
    if (L4_UNLIKELY (not cap.validate ().label ()))
      throw Loggable_exception (-L4_EINVAL, "Received capability is invalid");
    if (L4_UNLIKELY (_actions->count (name.data) != 0))
      throw Loggable_exception (
          -L4_EEXIST, fmt::format ("Action '{:s}' already exists", name.data));

    /* get the received capability */
    (*_actions)[name.data] = cap;
    if (L4_UNLIKELY (server_iface ()->realloc_rcv_cap (0) < 0))
      throw Loggable_exception (-L4_ENOMEM, "Failed to realloc_rcv_cap");
    return L4_EOK;
  }
};

struct Manager_Worker_Epiface
    : L4::Epiface_t<Manager_Worker_Epiface, MettEagle::Manager_Worker,
                    Manager_Base_Epiface>
{
protected:
  /* worker object that corresponds to this epiface */
  std::shared_ptr<Worker> _worker;

public:
  Manager_Worker_Epiface (
      std::shared_ptr<std::map<std::string, L4::Cap<L4Re::Dataspace> > >
          actions,
      L4::Cap<L4::Thread> thread, L4::Cap<L4::Scheduler> scheduler,
      std::shared_ptr<Worker> worker)
  {
    /* passed actions map from the client */
    _actions = actions;
    _thread = thread;
    _scheduler = scheduler;
    _worker = worker;
  }

  /**
   * Implementation of the signal method from the L4Re::Parent interface.
   *
   * @param[in]  sig  Signal to send
   * @param[in]  val  Value of the signal
   * @retval          0 on Success
   * @retval          <0 IPC error
   *
   * @note This function will be called from the uclibc on _exit of the
   * process.
   * _exit will set sig to be 0 and val will hold the exit code.
   *
   * @see l4re-core/uclibc/lib/uclibc/_exit.cc
   */
  long
  op_signal (L4Re::Parent::Rights, unsigned long sig, unsigned long val)
  {
    if (sig == 0) /*  exit -- why not SIGCHLD ? */
      {
        /* faas function probably threw an error   *
         * in this case value holds the error code */
        Log::error (fmt::format (
            "Worker finished with wrong exit! - it probably threw and "
            "error - Err: {:d}",
            val));
        // TODO return error to parent
        // TODO maybe better to delete cap??
        _worker->exit ("");
        L4Re::Env::env ()->task ()->release_cap (obj_cap ());
        // terminate ();

        //     delete this;

        /* do not send answer -- child shouldn't exist anymore */
        return -L4_ENOREPLY;
      }
    /* do nothing per default */
    return L4_EOK;
  }

  long
  op_exit (MettEagle::Manager_Worker::Rights,
           const L4::Ipc::String_in_buf<> &value)
  {
    Log::debug (fmt::format ("Worker exit: {:s}", value.data));
    // terminate ();
    _worker->exit (value.data);
    L4Re::Env::env ()->task ()->release_cap (obj_cap ());
    //     delete this;

    /* With -L4_ENOREPLY no answer will be send to the worker. Thus the worker
     * will keep waiting. */
    return -L4_ENOREPLY;
  }
};

long
Manager_Base_Epiface::op_action_invoke (MettEagle::Manager_Base::Rights,
                                        const L4::Ipc::String_in_buf<> &name,
                                        L4::Ipc::Array_ref<char> &ret)
{
  Log::debug (fmt::format ("Invoke action name='{:s}'", name.data));
  /* c++ maps dont have a map#contains */
  if (L4_UNLIKELY (_actions->count (name.data) == 0))
    throw Loggable_exception (
        -L4_EINVAL, fmt::format ("Action '{}' doesn't exist", name.data));

  /* cap can be unmapped anytime ... TODO add try catch or some error
   * handling
   */
  L4::Cap<L4Re::Dataspace> file = (*_actions)[name.data];
  if (L4_UNLIKELY (not file.validate ().label ()))
    throw Loggable_exception (-L4_EINVAL, "dataspace invalid");

  // TODO extract to own function
  std::string exit_value;
  {
    /* register the object in the server loop. This will create the        *
     * capability for the object and inform the server to route IPC there. */
    /**
     * Note: One needs to be very careful here. On deletion (at the end of the
     * scope) the smart capability will unmap its managed capability. This
     * unmap will be a systemcall itself and again mess up the utcb.
     *
     * To prevent the corruption of returned values the cap should be deleted
     * before they are set.
     */
    auto cap = L4Re::chkcap (
        L4Re::Util::make_ref_del_cap<MettEagle::Manager_Worker> (),
        "Failed to alloc cap", -L4_ENOMEM);
    /* Pass the IPC gate cap to process as parent */
    auto alloc = L4Re::Util::cap_alloc.alloc<L4::Factory>();
    L4Re::chksys(l4_msgtag_t(L4Re::Env::env()->user_factory()->create(alloc) << l4_mword_t(1024 * 1024))); // TODO query from client 
    auto worker = std::make_shared<Worker> (file, cap, _scheduler, alloc);
    /* create the ipc handler for started process */
    auto worker_epiface = std::make_unique<Manager_Worker_Epiface> (
        _actions, _thread, _scheduler, worker);
    /* link capability to ipc gate */
    L4Re::LibLog::chksys (
        L4Re::Env::env ()->factory ()->create_gate (
            cap.get (), _thread, l4_umword_t (worker_epiface.get ())),
        "Failed to create gate");
    worker_epiface->set_server (new L4::Ipc_svr::Default_loop_hooks (),
                                cap.get ());

    /* start worker */
    worker->set_argv_strings ({ name.data });
    worker->set_envp_strings ({ "PKGNAME=Worker    ", "LOG_LEVEL=DEBUG" });
    worker->add_initial_capability (
        L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync"), "log_sync",
        L4_CAP_FPAGE_RWSD);
    worker->launch ();

    /**
     * ========================= Server loop =========================
     * The following code implements a simple server loop. This loop has
     * no demand allocation (-> can't receive capabilities) and will only
     * dispatch to a single Epiface object.
     * Nonetheless this loop is necessary to receive from and reply to a
     * specific capability. In order to keep the reply capability of the
     * client.
     */
    l4_msgtag_t msg = L4Re::chkipc (
        l4_ipc_receive (worker->_thread.cap (), l4_utcb (), L4_IPC_NEVER),
        "Worker ipc failed.");

    while (true)
      {
        /* call the corresponding function of the epiface */
        l4_msgtag_t reply = worker_epiface->dispatch (
            msg, 0 /* rights dont matter */, l4_utcb ());
        /* Note: be careful can't invoke any ipc between dispatch an ipc_call
         * (do not modify utcb) */
        /* the exit handler will exit the worker */
        if (not worker->alive ())
          break;
        /* no exit received -> wait for next RPC */
        msg = l4_ipc_call (worker->_thread.cap (), l4_utcb (), reply,
                           L4_IPC_NEVER);
      }
    exit_value = worker->get_exit_value ();
    /* check if utcb buffer is large enough */
    if (L4_UNLIKELY (exit_value.length () >= ret.length))
      throw Loggable_exception (-L4_EMSGTOOLONG,
                                "The utcb buffer is too small!");
    /* delete smart pointers */
  }

  /* cp message into buffer */
  memcpy (ret.data, exit_value.c_str (), exit_value.length () + 1);
  ret.length = exit_value.length () + 1;
  return L4_EOK;
}

/**
 * Cpu bitmap of cpus accessible to the manager process
 * that are not currently assigned to client.
 */
l4_sched_cpu_set_t available_cpus;

/**
 * This will select a cpu for a newly connected client
 *
 * @return l4_umword_t  Cpu bitmap with 1 selected cpu
 */
static l4_umword_t
select_client_cpu ()
{
  l4_umword_t selected = 0x1;
  if (L4_UNLIKELY (not available_cpus.map))
    throw Loggable_exception (-L4_ENOENT, "No cpu available");
  while (not(selected & available_cpus.map))
    selected <<= 1;
  available_cpus.map &= ~selected;
  return selected;
}

struct Manager_Registry_Epiface
    : L4::Epiface_t<Manager_Registry_Epiface, MettEagle::Manager_Registry>
{
  long
  op_register_client (
      MettEagle::Manager_Registry::Rights,
      L4::Ipc::Cap<MettEagle::Manager_Client> &manager_ipc_gate)
  {
    Log::debug ("Registering client");

    // TODO cap alloc is not thread safe
    
    /**
     * This function will create a new thread (just like pthreads) that is
     * responsible for handling this particular client.
     */

#if 0
    /**
     * To have a more fine grained control over the thread no library (e.g.
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
    //   Log::debug("called callback");
    // });

    auto stack_cap = L4Re::chkcap (
        L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> (), "alloc cap");

    // alloc single page for the thread stack
    int stack_size = 4096;
    L4Re::chksys (env->mem_alloc ()->alloc (stack_size, stack_cap));

    l4_addr_t stack_addr;
    L4Re::chksys (
        env->rm ()->attach (&stack_addr, stack_size,
                            L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                            L4::Ipc::make_cap_rw (stack_cap), L4_STACK_ALIGN),
        "attaching stack vma");

    L4Re::chksys (
        client_thread->ex_regs (
            (l4_addr_t)client_function /* some address */,
            l4_align_stack_for_direct_fncall ((unsigned long)stack_addr), 0),
        "start app thread");

    // run thread
    Log::debug ("running test thread");

    L4Re::chksys (env->scheduler ()->run_thread (
        client_thread, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));
#endif
#if 1

    // create scheduler for specific client

    auto sched_cap = L4Re::Util::cap_alloc.alloc<L4::Scheduler> ();

    l4_umword_t limit = L4_SCHED_MAX_PRIO;
    l4_umword_t offset = L4_SCHED_MIN_PRIO;

    /**
     * "Bitmap" with only one bit set. So each thread will run on it's own cpu.
     * The same 'Scheduler' will be used for the started processes of the
     * client.
     */
    l4_umword_t bitmap = select_client_cpu ();
    Log::debug (fmt::format ("Selected cpu {:x}", bitmap));

    L4Re::chksys (
        l4_msgtag_t (
            L4Re::Env::env ()->user_factory ()->create<L4::Scheduler> (
                sched_cap)
            << l4_mword_t (limit) << l4_mword_t (offset)
            << l4_umword_t (bitmap)),
        "Failed to create scheduler");

    pthread_t pthread;
    pthread_attr_t attr;

    pthread_attr_init (&attr);
    attr.create_flags |= PTHREAD_L4_ATTR_NO_START;
    L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> *client_server;
    int failed = pthread_create (
        &pthread, &attr,
        [] (void *arg) -> void * {
          auto client_server
              = (L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> **)
                  arg;
          /* Wait until registry server is created */
          Log::info ("Start client ipc server");

          (*client_server)
              ->internal_loop (
                  Exc_log_dispatch<L4Re::Util::Object_registry &> (
                      *(*client_server)->registry ()),
                  l4_utcb ());
        },
        &client_server);
    if (failed)
      {
        Log::error ("failed to create thread");
      }

    pthread_attr_destroy (&attr);

    client_server
        = new L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> (
            L4::Cap<L4::Thread> (pthread_l4_cap (pthread)),
            L4Re::Env::env ()->factory ());
    /* create new object handling the requests of this client */
    /* TODO handle deallocation */
    auto epiface = new Manager_Client_Epiface (
        L4::Cap<L4::Thread> (pthread_l4_cap (pthread)), sched_cap, bitmap);
    /* register the object in the server loop. This will create the        *
     * capability for the object and inform the server to route IPC there. */
    L4::Cap<void> cap = client_server->registry ()->register_obj (epiface);
    if (L4_UNLIKELY (not cap.is_valid ()))
      {
        client_server->registry ()->unregister_obj (epiface);
        // TODO destroy thread -- done by discarding the handle?
        throw Loggable_exception (-L4_ENOMEM,
                                  "Failed to register client IPC gate");
      }
    /* notify thread */

    L4::Cap<L4::Thread> thread{ pthread_l4_cap (pthread) };
    // TODO check if affinity is used even if bitmap only contains one bit
    L4Re::chksys(sched_cap->run_thread (thread, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));
    manager_ipc_gate = L4::Cap<MettEagle::Manager_Client> ();
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
#endif
#if 0
    /* sync client and server */
    std::condition_variable sync;
    /* used for sync */
    std::mutex mtx;

    /*
     * create a new server that will handle only this client
     * (and its processes)
     */
    /*
     * Br_manager is necessary to handle the demand of cap slots. They
     * are used e.g. to receive the Dataspace of a client while creating
     * a new action.
     */
    L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> *client_server
        = nullptr;

    /* start new thread handling this specific client */
    std::thread client_handler ([&mtx, &sync, &client_server] {
      /* Wait until registry server is created */
      std::unique_lock<std::mutex> lock{ mtx }; // acquire lock
      sync.wait (lock, [&client_server] { return client_server != nullptr; });
      lock.unlock ();
      Log::info ("Start client ipc server");
      client_server->internal_loop (
          Exc_log_dispatch<L4Re::Util::Object_registry &> (
              *client_server->registry ()),
          l4_utcb ());
    });

    Log::debug ("created thread %ld", std::L4::thread_cap (client_handler));

    std::unique_lock<std::mutex> lock{ mtx }; // acquire lock
    client_server
        = new L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> (
            std::L4::thread_cap (client_handler),
            L4Re::Env::env ()->factory ());
    /* create new object handling the requests of this client */
    /* TODO handle deallocation */
    auto epiface
        = new Manager_Client_Epiface (std::L4::thread_cap (client_handler));
    /* register the object in the server loop. This will create the        *
     * capability for the object and inform the server to route IPC there. */
    L4::Cap<void> cap = client_server->registry ()->register_obj (epiface);
    if (L4_UNLIKELY (not cap.is_valid ()))
      {
        client_server->registry ()->unregister_obj (epiface);
        // TODO destroy thread -- done by discarding the handle?
        throw Loggable_exception (-L4_ENOMEM,
                                  "Failed to register client IPC gate");
      }
    /* notify thread */
    lock.unlock ();
    sync.notify_one ();
    /*
     * Note: The thread might start the server loop after the ipc call returned
     * to the client. In case the client sends a request before the server loop
     * is started it will be blocked by the ipc until the thread calls
     * server::loop() and thereby start an open wait.
     */

    /* Pass the IPC gate back to the client as output argument. */
    manager_ipc_gate = epiface->obj_cap ();

    /* separate thread from the 'client_handler' object */
    client_handler.detach ();
#endif
    return L4_EOK;
  }
};

/**
 * Absolutely no clue what this does..
 * Something with the kip ...
 * KIP === Kernel Interface Page
 *
 * Used by the loader somehow
 */
l4re_aux_t *l4re_aux;

/**
 * This object will handle registration RPCs from clients.
 */
L4Re::Util::Registry_server<> register_server;

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

    l4_umword_t cpu_max;
    auto cpus = l4_sched_cpu_set (0, 0);

    L4Re::chksys (L4Re::Env::env ()->scheduler ()->info (&cpu_max, &cpus),
                  "failed to query scheduler info");

    available_cpus = cpus;
    Log::debug (
        fmt::format ("scheduler->info: available cpus {:#b}", cpus.map));

    /*
     * Associate the 'server' endpoint that was already
     * 'reserved' by ned with a newly created interface
     * implementation
     */
    L4Re::chkcap (register_server.registry ()->register_obj (
                      new Manager_Registry_Epiface (), "server"),
                  "Couldn't register service, is there a 'server' in "
                  "the caps table?");

    Log::info ("Starting Mett-Eagle server!");

    // start server loop -- loop will not return!
    // started with custom dispatch to log errors
    register_server.internal_loop (
        Exc_log_dispatch<L4Re::Util::Object_registry &> (
            *register_server.registry ()),
        l4_utcb ());
  }
catch (L4::Runtime_error &e)
  {
    /**
     * Catch all errors (e.g. from chkcap) and log some message
     */
    Log::fatal (fmt::format ("{}", e));
    return e.err_no ();
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    Log::fatal (fmt::format ("{}", e));
    return e.err_no ();
  }