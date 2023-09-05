/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "manager_base.h"
#include "manager_worker.h"
#include "worker.h"

#include <l4/sys/cxx/ipc_server_loop>

long
Manager_Base_Epiface::op_action_invoke (MettEagle::Manager_Base::Rights,
                                        const L4::Ipc::String_in_buf<> &_name,
                                        const L4::Ipc::String_in_buf<> &arg,
                                        L4::Ipc::Array_ref<char> &ret,
                                        MettEagle::Config cfg,
                                        MettEagle::Metadata &data)
{
  const char *name = _name.data;

  /* data store on stack to prevent corruption of values inside utcb
   * see next 'Note' for more information */
  MettEagle::Metadata meta_data;

  log<DEBUG> ("Invoke action name='{:s}'", name);
  /* c++ maps dont have a map#contains */
  if (L4_UNLIKELY (_actions->count (name) == 0))
    throw Loggable_exception (-L4_EINVAL, "Action '{}' doesn't exist", name);

  /* cap can be unmapped anytime ... maybe we should create a local copy on
   * action create?? ... TODO add try catch or some error handling */
  auto file = (*_actions)[name];
  if (L4_UNLIKELY (not file.validate ().label ()))
    throw Loggable_exception (-L4_EINVAL, "dataspace invalid");

  // TODO extract to own function
  std::string exit_value;
  {
    /**
     * Note: One needs to be very careful here. On deletion (at the end of the
     * scope) the smart capability will unmap its managed capability. This
     * unmap will be a systemcall itself and again mess up the utcb.
     *
     * To prevent the corruption of returned values the cap should be deleted
     * before the values are set.
     */

    /* IPC gate cap passed to process as parent capability */
    auto parent_ipc_cap
        = chkcap (L4Re::Util::make_shared_cap<MettEagle::Manager_Worker> (),
                  "alloc parent cap", -L4_ENOMEM);

    /* only necessary to ensure the limited allocator is freed at the end of
     * the scope */
    L4Re::Util::Shared_cap<L4::Factory> allocator;
    if (cfg.memory_limit == 0)
      {
        /* default to own user factory == 'unlimited' memory */
        /* user_factory wont be unmapped by the Shared_cap since it is not
         * managed by the Util::cap_alloc */
        allocator = L4Re::Util::Shared_cap<L4::Factory> (
            L4Re::Env::env ()->user_factory ());
      }
    else
      {
        /* create limited allocator if limit is specified */
        allocator = L4Re::Util::make_shared_cap<L4::Factory> ();
        chksys (
            l4_msgtag_t (
                L4Re::Env::env ()->user_factory ()->create (allocator.get ())
                << cfg.memory_limit),
            "create limited allocator");
      }

    // TODO make this cleaner ... take is necessary since the creation of a new
    // shared_cap from the casted cap wont increase ref count
    L4Re::Util::cap_alloc.take (parent_ipc_cap.get ());
    auto worker = std::make_shared<Worker> (
        file,
        L4Re::Util::Shared_cap<L4Re::Parent> (
            L4::cap_cast<L4Re::Parent> (parent_ipc_cap.get ())),
        _scheduler, allocator);
    /* create the ipc handler for started process */
    auto worker_epiface = std::make_unique<Manager_Worker_Epiface> (
        _actions, _thread, _scheduler, worker);
    /* link parent capability to ipc gate */
    chksys (L4Re::Env::env ()->factory ()->create_gate (
                parent_ipc_cap.get (), _thread,
                l4_umword_t (worker_epiface.get ())),
            "Failed to create gate");
    auto worker_server = std::make_unique<L4::Ipc_svr::Default_loop_hooks> ();
    worker_epiface->set_server (worker_server.get (), parent_ipc_cap.get ());

    /* start worker */
    /* pass data as first argument string */
    worker->set_argv_strings ({ arg.data });
    worker->set_envp_strings ({ "PKGNAME=Worker    ", "LOG_LEVEL=31" });
    /* pass semaphore to sync log messages -- TODO can be removed for final
     * benchmarking */
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
     * ========================== Server loop ==========================
     *
     * The following code implements a simple server loop. This loop has
     * no demand allocation (-> can't receive capabilities) and will only
     * dispatch to a single Epiface object.
     * Nonetheless this loop is necessary to receive from and reply to a
     * specific capability. In order to keep the reply capability of the
     * client unchanged.
     */

    // TODO maybe add some timeout or heartbeat interval to prevent malicious
    // clients
    l4_msgtag_t msg = chkipc (
        l4_ipc_receive (worker->_thread.cap (), l4_utcb (), L4_IPC_NEVER),
        "Worker ipc failed.");

    while (true)
      {
        /* call the corresponding function of the epiface */
        l4_msgtag_t reply = worker_epiface->dispatch (
            msg, 0 /* rights dont matter */, l4_utcb ());
        /* Note: be careful can't invoke any ipc between dispatch and ipc_call
         * (do not modify utcb) */

        /* the exit handler (invoked by the dispatch) will exit the worker */
        if (not worker->alive ())
          break;
        /* no exit received -> wait for next RPC */
        msg = l4_ipc_call (worker->_thread.cap (), l4_utcb (), reply,
                           L4_IPC_NEVER); /* use compound send and receive */
      }
    // TODO handle error exit
    exit_value = worker->get_exit_value ();
    /* check if utcb buffer is large enough -- TODO is this necessary?*/
    if (L4_UNLIKELY (exit_value.length () >= ret.length))
      throw Loggable_exception (-L4_EMSGTOOLONG,
                                "The utcb buffer is too small!");

    /* fill metadata */
    meta_data.start = worker_epiface->start;
    meta_data.end = worker_epiface->end;

    /* delete smart pointers */
  }

  /* set return values */
  data = meta_data;
  memcpy (ret.data, exit_value.c_str (), exit_value.length () + 1);
  ret.length = exit_value.length () + 1;
  return L4_EOK;
}