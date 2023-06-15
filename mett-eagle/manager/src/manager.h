/*
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>

/**
 * Ipc server of the mett-eagle manager
 *
 * Br_manager necessary to handle demand of
 * receiving capability from client.
 */
extern L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

#include <l4/mett-eagle/manager>
#include <map>
extern std::map<L4::Cap<MettEagle::Manager_Base>, L4::Cap<MettEagle::Client> >
    gate_to_client;

#include <memory>
#include <l4/sys/cxx/ipc_epiface>

/**
 * Implementation of the base interface.
 * These functions will be called by both clients and workers.
 */
struct Manager_Base_Epiface : L4::Epiface_t0<MettEagle::Manager_Base>
{
protected:
  /**
   * @brief Per client name for dataspace resolution
   *
   * This map will hold all registered actions of a client and also provide
   * them to the workers started by this specific client only.
   */
  std::shared_ptr<std::map<std::string, L4::Cap<L4Re::Dataspace> > > _actions;

public:
  /**
   * @brief Invoke a serverless function
   *
   * @param[in] name  Name of the serverless function to invoke
   * @param[out] res  Result of the function
   * @return          L4_EOK on success
   * @return          -L4_EINVAL if the L4Re::Dataspace of the action is no
   *                  longer valid
   */
  long op_action_invoke (MettEagle::Manager_Base::Rights,
                         const L4::Ipc::String_in_buf<> &name);
};

struct Manager_Client_Epiface
    : L4::Epiface_t<Manager_Client_Epiface, MettEagle::Manager_Client,
                    Manager_Base_Epiface>
{
  Manager_Client_Epiface ()
  {
    /* _actions map will be create by the clients constructor and only passed
     * to each worker */
    _actions = std::make_shared<
        std::map<std::string, L4::Cap<L4Re::Dataspace> > > ();
  }

  /**
   * @brief Create a new 'action'
   *
   * This will search for the 'pathname' in the L4Re::Util::Env_ns
   * and create a new action with the content of the file.
   *
   * @TODO change to pass Dataspace instead of file name!!
   *
   * @param[in] name      Name that will identify the action
   * @param[in] pathname  File name
   * @return              L4_EOK on success
   * @return              -L4_ENOENT if the given pathname does not exist
   * inside the Env_nv
   */
  long op_action_create (MettEagle::Manager_Client::Rights,
                         const L4::Ipc::String_in_buf<> &name,
                         L4::Ipc::Snd_fpage file);
};

struct Manager_Worker_Epiface
    : L4::Epiface_t<Manager_Worker_Epiface, MettEagle::Manager_Worker,
                    Manager_Base_Epiface>
{
  Manager_Worker_Epiface (
      std::shared_ptr<std::map<std::string, L4::Cap<L4Re::Dataspace> > >
          actions)
  {
    /* passed actions map from the client */
    _actions = actions;
  }

  /**
   * Implementation of the signal method from the L4Re::Parent interface.
   *
   * This function will be automatically called by workers, clients do not
   * need to use it.
   *
   * @param[in]  sig  Signal to send
   * @param[in]  val  Value of the signal
   * @retval 0   Success
   * @retval <0  IPC error
   *
   * @note This function will be called from the uclibc on _exit of the
   * process. Thus it can be used to 'wait' for a child.
   * On _exit will set sig to be 0 and val will hold the exit code.
   *
   * @see l4re-core/uclibc/lib/uclibc/_exit.cc
   */
  long op_signal (L4Re::Parent::Rights, unsigned long sig, unsigned long val);

  /**
   * @brief Faas specific exit functions that workers should use
   *
   * This function behaves like a normal _exit but it provides the ability to
   * return a string instead of an integer exit code
   *
   * @param[in] value  String exit value
   */
  long op_exit (MettEagle::Manager_Worker::Rights,
                const L4::Ipc::String_in_buf<> &value);
};

/**
 * Implementation of the ManagerRegistry interface which clients can
 * use to register themselves.
 * Also creating a global object which will be 'passed' to the clients.
 */
struct Manager_Registry_Epiface
    : L4::Epiface_t<Manager_Registry_Epiface, MettEagle::Manager_Registry>
{
  /**
   * @brief Register a new client.
   *
   * This method will send an Ipc_gate from a new client to the manager. This
   * gate will be used to answer requests of this specific client.
   *
   * The server will reply with a newly created Ipc_gate which should be used
   * by the client for all subsequent requests. This is necessary for the
   * manager to identify which message came from which client and where to send
   * the reply.
   *
   * @param[in]  client_ipc_gate   Client Ipc_gate that the manager will call
   *                               (e.g. if an action finished)
   * @param[out] manager_ipc_gate  Manager Ipc_gate that should be used for all
   *                               subsequent interaction
   * @return                       L4_EOK on success
   * @return                       -L4_EINVAL if the received capability is
   *                               invalid
   * @return                       -L4_ENOMEM if no new capability could be
   *                               allocated
   */
  long op_register_client (
      MettEagle::Manager_Registry::Rights, L4::Ipc::Snd_fpage client_ipc_gate,
      L4::Ipc::Cap<MettEagle::Manager_Client> &manager_ipc_gate);
};