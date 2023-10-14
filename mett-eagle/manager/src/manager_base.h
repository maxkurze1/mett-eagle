/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
/**
 * @file
 * Epiface declaration for the base interface
 * this epiface will be inherited by the client
 * and the worker epiface.
 *
 * @see <l4/mett-eagle/base>
 */

#pragma once

#include "manager.h"

#include <l4/mett-eagle/base>
#include <l4/mett-eagle/client>

#include <l4/re/dataspace>
#include <l4/re/util/shared_cap>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/scheduler>
#include <l4/sys/thread>

#include <map>
#include <memory>
#include <string>

struct Action
{
  L4Re::Util::Shared_cap<L4Re::Dataspace> ds;
  MettEagle::Language lang;
};

struct Manager_Base_Epiface : L4::Epiface_t0<MettEagle::Manager_Base>
{
protected:
  /**
   * @brief Per client namespace (map) for dataspace resolution
   *
   * This map will hold all registered actions of a client and also provide
   * them to the workers started by this specific client only.
   *
   * It is implemented as a shared_ptr since it will be referenced by client
   * epifaces and worker epifaces
   */
  std::shared_ptr<std::map<std::string, Action> > _actions;

  /**
   * @brief The client specific thread that will execute all actions
   *
   * This cap is not shared ... the deallocation of the thread capability will
   * be left for pthreads
   */
  L4::Cap<L4::Thread> _thread;

  /**
   * @brief The Scheduler object that is used by the thread and worker
   * processes
   *
   * shared by client epifaces and worker epifaces
   */
  L4Re::Util::Shared_cap<L4::Scheduler> _scheduler;

public:
  long op_action_invoke (MettEagle::Manager_Base::Rights,
                         const L4::Ipc::String_in_buf<> &name,
                         const L4::Ipc::String_in_buf<> &arg,
                         L4::Ipc::Array_ref<char> &ret, MettEagle::Config cfg,
                         MettEagle::Metadata &data);
};