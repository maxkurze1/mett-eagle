/*
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/mett-eagle/manager>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <map>
#include <memory>
#include <l4/mett-eagle/alias>

/**
 * Ipc server of the mett-eagle manager
 *
 * Br_manager necessary to handle capability
 * demand
 */
extern L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

extern std::map<L4::Cap<MettEagle::Manager_Base>,
                L4::Cap<MettEagle::Client> >
    gate_to_client;

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
  long op_action_invoke (MettEagle::Manager_Base::Rights,
                         const L4::Ipc::String_in_buf<> &name);
};

struct Manager_Client_Epiface
    : L4::Epiface_t<Manager_Client_Epiface, MettEagle::Manager_Client,
                    Manager_Base_Epiface>
{
  Manager_Client_Epiface ()
  {
    /* _actions map will be create by the clients and only passed *
     * to each worker                                             */
    _actions = std::make_shared<
        std::map<std::string, L4::Cap<L4Re::Dataspace> > > ();
  }

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
  long op_signal (L4Re::Parent::Rights, unsigned long sig, unsigned long val);

  long op_exit (MettEagle::Manager_Worker::Rights,
                const L4::Ipc::String_in_buf<> &value);
};

struct Manager_Registry_Epiface
    : L4::Epiface_t<Manager_Registry_Epiface,
                    MettEagle::Manager_Registry>
{
  long op_register_client (
      MettEagle::Manager_Registry::Rights,
      L4::Ipc::Snd_fpage client_ipc_gate,
      L4::Ipc::Cap<MettEagle::Manager_Client> &manager_ipc_gate);
};