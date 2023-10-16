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

#include <l4/re/util/env_ns>
#include <l4/sys/cxx/ipc_server_loop>

#include <l4/sys/debugger.h>

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
  // log<DEBUG> ("Invoke action name='{}'", name);
  /* c++ maps dont have a map#contains */
  if (L4_UNLIKELY (_actions->count (name) == 0))
    throw Loggable_exception (-L4_EINVAL, "Action '{}' doesn't exist", name);

  /* cap can be unmapped anytime ... maybe we should create a local copy on
   * action create?? ... TODO add try catch or some error handling */
  auto action = (*_actions)[name];
  if (L4_UNLIKELY (not action.ds.validate ().label ()))
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

    L4Re::Util::Shared_cap<L4Re::Dataspace> worker_ds;
    switch (action.lang)
      {
      case MettEagle::Language::BINARY:
        /* in case the received dataspace already contains the binary, it is
         * started directly */
        worker_ds = action.ds;
        break;
        /* if it need a runtime the correct one should be selected */
      case MettEagle::Language::PYTHON:
        worker_ds = L4Re::Util::Shared_cap<L4Re::Dataspace> (
            L4Re::Util::Env_ns{}.query<L4Re::Dataspace> (
                "rom/python-faas2.7")); // TODO probably not safe to put into a
                                        // shared cap
        if (L4_UNLIKELY (not worker_ds.is_valid ()))
          throw Loggable_exception (-L4_EINVAL,
                                    "Couldn't find file 'rom/python-faas2.7'");
        break;
      }

    auto worker = std::make_shared<Worker> (
        worker_ds, parent_ipc_cap.get (), _scheduler.get (), allocator.get ());
    /* create the ipc handler for started process */
    auto worker_epiface = std::make_unique<Manager_Worker_Epiface> (
        _actions, _thread, _scheduler, worker);
    /* link parent capability to ipc gate */
    chksys (L4Re::Env::env ()->factory ()->create_gate (
                parent_ipc_cap.get (), _thread,
                l4_umword_t (worker_epiface.get ())),
            "Failed to create gate");
    // l4_debugger_set_object_name (parent_ipc_cap.cap (), "wrkr->mngr");
    auto worker_server = std::make_unique<L4::Ipc_svr::Default_loop_hooks> ();
    worker_epiface->set_server (worker_server.get (), parent_ipc_cap.get ());

    /* start worker */
    /* pass data as first argument string */
    worker->set_argv_strings ({ arg.data });
    worker->set_envp_strings ({ "PKGNAME=Worker    ", "LOG_LEVEL=31" });

    worker->add_initial_capability (
        L4Re::Env::env ()->get_cap<L4Re::Namespace> ("rom"), "rom", L4_cap_fpage_rights::L4_CAP_FPAGE_RW);
    if (action.lang != MettEagle::Language::BINARY)
      worker->add_initial_capability (action.ds.get (), "function",
                                      L4_cap_fpage_rights::L4_CAP_FPAGE_RW);

    /**
     * The corresponding 'end' measurement will be taken in the exit ipc
     * handler function
     */
    meta_data.start_worker = std::chrono::high_resolution_clock::now ();

    worker->launch ();
    // l4_debugger_set_object_name (worker->_task.cap (), "wrkr");
    // l4_debugger_set_object_name (worker->_thread.cap (), "wrkr");
    // l4_debugger_set_object_name (worker->_rm.cap (), "wrkr rm");

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

    meta_data.end_worker = std::chrono::high_resolution_clock::now ();

    // TODO return error code to parent
    if (worker->exited_with_error ())
      throw Loggable_exception (-L4_EFAULT, "Worker exited with error");
    exit_value = worker->get_exit_value ();
    auto worker_data = worker_epiface->_metadata;

    meta_data.start_runtime = worker_data.start_runtime;
    meta_data.start_function = worker_data.start_function;
    meta_data.end_function = worker_data.end_function;
    meta_data.end_runtime = worker_data.end_runtime;

    /* check if utcb buffer is large enough -- TODO is this necessary?*/
    if (L4_UNLIKELY (exit_value.length () >= ret.length))
      throw Loggable_exception (-L4_EMSGTOOLONG,
                                "The utcb buffer is too small!");

    /* delete smart pointers */
  }

  /* set return values */
  data = meta_data;
  memcpy (ret.data, exit_value.c_str (), exit_value.length () + 1);
  ret.length = exit_value.length () + 1;
  return L4_EOK;
}