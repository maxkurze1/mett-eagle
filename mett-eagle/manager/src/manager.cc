/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "app_model.h"
#include "worker.h"

#include <bitset>
#include <map>
#include <memory>
#include <pthread-l4.h>
#include <pthread.h>
#include <string>
#include <strings.h> // needed for ffsll

#include <l4/fmt/core.h>
#include <l4/liblog/error_helper>
#include <l4/liblog/exc_log_dispatch>
#include <l4/liblog/log>
#include <l4/liblog/loggable-exception>
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
                         const L4::Ipc::String_in_buf<> &arg,
                         L4::Ipc::Array_ref<char> &ret, MettEagle::Config cfg,
                         MettEagle::Metadata &data);
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
                    const L4::Ipc::String_in_buf<> &_name,
                    L4::Ipc::Snd_fpage file)
  {
    const char *name = _name.data;
    Log::debug ("Create action name='{:s}' file passed='{}'",
                             name, file.cap_received ());
    if (L4_UNLIKELY (not file.cap_received ()))
      throw Loggable_exception (-L4_EINVAL, "No dataspace cap received");
    auto cap = server_iface ()->rcv_cap<L4Re::Dataspace> (0);
    if (L4_UNLIKELY (not cap.validate ().label ()))
      throw Loggable_exception (-L4_EINVAL, "Received capability is invalid");
    if (L4_UNLIKELY (_actions->count (name) != 0))
      throw Loggable_exception (
          -L4_EEXIST, fmt::format ("Action '{:s}' already exists", name));

    /* get the received capability */
    (*_actions)[name] = cap;
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
  /* start will be measured as short as possible before worker execution */
  /* though it will still be measured before the elf launcher starts */
  std::chrono::time_point<std::chrono::high_resolution_clock> start;

  /* start will be measured as short as possible after worker exit */
  /* this will be measured in the corresponding rpc handler */
  std::chrono::time_point<std::chrono::high_resolution_clock> end;

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
        this->end = std::chrono::high_resolution_clock::now ();

        auto err = static_cast<int> (val);
        /* faas function probably threw an error   *
         * in this case value holds the error code */
        Log::error (
            "Worker finished with wrong exit! - {:s} (=exit: {:d})",
            l4sys_errtostr (err), err);
        // TODO return error to parent
        // TODO maybe better to delete cap??
        _worker->exit ("");
        L4Re::Env::env ()->task ()->release_cap (obj_cap ());
        // terminate ();

        // delete this;

        /* do not send answer -- child shouldn't exist anymore */
        return -L4_ENOREPLY;
      }
    /* do nothing per default */
    return L4_EOK;
  }

  long
  op_exit (MettEagle::Manager_Worker::Rights,
           const L4::Ipc::String_in_buf<> &_value)
  {
    this->end = std::chrono::high_resolution_clock::now ();

    const char *value = _value.data;

    Log::debug ("Worker exit: {:s}", value);
    // terminate ();
    _worker->exit (value);
    L4Re::Env::env ()->task ()->release_cap (obj_cap ());
    //     delete this;

    /* With -L4_ENOREPLY no answer will be send to the worker. Thus the worker
     * will keep waiting. */
    return -L4_ENOREPLY;
  }
};

long
Manager_Base_Epiface::op_action_invoke (MettEagle::Manager_Base::Rights,
                                        const L4::Ipc::String_in_buf<> &_name,
                                        const L4::Ipc::String_in_buf<> &arg,
                                        L4::Ipc::Array_ref<char> &ret,
                                        MettEagle::Config cfg,
                                        MettEagle::Metadata &data)
{
  const char *name = _name.data;

  Log::debug ("Invoke action name='{:s}'", name);
  /* c++ maps dont have a map#contains */
  if (L4_UNLIKELY (_actions->count (name) == 0))
    throw Loggable_exception (-L4_EINVAL,
                              fmt::format ("Action '{}' doesn't exist", name));

  /* cap can be unmapped anytime ... TODO add try catch or some error
   * handling
   */
  L4::Cap<L4Re::Dataspace> file = (*_actions)[name];
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
    L4::Cap<L4::Factory> allocator;
    if (cfg.memory_limit == 0)
      {
        allocator = L4Re::Env::env ()->user_factory ();
      }
    else
      {
        allocator = L4Re::Util::cap_alloc.alloc<L4::Factory> ();
        L4Re::chksys (
            l4_msgtag_t (L4Re::Env::env ()->user_factory ()->create (allocator)
                         << cfg.memory_limit),
            "create limited allocator");
      }
    auto worker = std::make_shared<Worker> (file, cap, _scheduler, allocator);
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
    worker->set_argv_strings ({ arg.data });
    worker->set_envp_strings ({ "PKGNAME=Worker    ", "LOG_LEVEL=DEBUG" });
    worker->add_initial_capability (
        L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync"), "log_sync",
        L4_CAP_FPAGE_RWSD);

    /**
     * The corresponding 'end' measurement will be taken in the exit ipc
     * handler function
     */
    worker_epiface->start = std::chrono::high_resolution_clock::now ();

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
        /* Note: be careful can't invoke any ipc between dispatch and ipc_call
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

    /* fill metadata */
    data.start = worker_epiface->start;
    data.end = worker_epiface->end;

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
std::bitset<sizeof (l4_sched_cpu_set_t::map) * 8> available_cpus;

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

    /** scheduler with only one selected cpu enabled for the new thread */
    auto sched_cap = L4Re::Util::cap_alloc.alloc<L4::Scheduler> ();
    l4_mword_t limit = L4_SCHED_MAX_PRIO;
    l4_mword_t offset = L4_SCHED_MIN_PRIO;

    /**
     * "Bitmap" with only one bit set. So each thread will run on it's own cpu.
     * The same 'Scheduler' will be used for the started processes of the
     * client.
     */
    l4_umword_t bitmap = select_client_cpu ();
    Log::debug ("Selected cpu {:b}", bitmap);

    L4Re::chksys (
        l4_msgtag_t (
            L4Re::Env::env ()->user_factory ()->create<L4::Scheduler> (
                sched_cap)
            << limit << offset << bitmap),
        "Failed to create scheduler");

    pthread_t pthread;
    pthread_attr_t attr;

    pthread_attr_init (&attr);
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

          Log::info ("Start client ipc server");

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
        delete client_server;
        delete client_server_pointer;
        // TODO destroy thread -- cancel?
        auto canceled = pthread_cancel (pthread) == 0;
        throw Loggable_exception (
            -L4_ENOMEM,
            fmt::format (
                "Failed to register client IPC gate, thread_canceled={}",
                canceled));
      }

    /*
     * start the thread and wait until the pointer to 'client_server' was
     * copied into it before leaving the function and discarding the stack
     */
    L4::Cap<L4::Thread> thread{ pthread_l4_cap (pthread) };
    // TODO check if affinity is used even if bitmap only contains one bit
    L4Re::chksys (sched_cap->run_thread (
        thread, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));
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
 * This object will handle the registration of clients.
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

    /**
     * Query available cpu-set which can be distributed to clients
     */
    l4_umword_t cpu_max;
    l4_sched_cpu_set_t cpus{ l4_sched_cpu_set (0, 0) };
    L4Re::chksys (L4Re::Env::env ()->scheduler ()->info (&cpu_max, &cpus),
                  "failed to query scheduler info");

    /* update global bitmap */
    available_cpus = cpus.map;

    Log::info (
        "Scheduler info (available cpus) :: {:0{}b} => {:d}/{:d}",
                     cpus.map, cpu_max, available_cpus.count (), cpu_max);

    /*
     * Associate the 'server' endpoint that was already
     * 'reserved' by ned with a newly created interface
     * implementation
     */
    L4Re::chkcap (register_server.registry ()->register_obj (
                      new Manager_Registry_Epiface (), "server"),
                  "Couldn't register service, is there a 'server' in "
                  "the caps table?");

    Log::info ("Starting Mett-Eagle registry server!");

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
    Log::fatal ("{}", e);
    return e.err_no ();
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    Log::fatal ("{}", e);
    return e.err_no ();
  }