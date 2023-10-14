/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
/**
 * @file
 * Epiface declaration for the client interface
 *
 * @see <l4/mett-eagle/client>
 */

#pragma once

#include "manager.h"
#include "manager_base.h"

#include <l4/mett-eagle/client>

#include <l4/re/util/shared_cap>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/scheduler>
#include <l4/sys/thread>

struct Manager_Client_Epiface
    : L4::Epiface_t<Manager_Client_Epiface, MettEagle::Manager_Client,
                    Manager_Base_Epiface>
{
public:
  Manager_Client_Epiface (L4::Cap<L4::Thread> thread,
                          L4Re::Util::Shared_cap<L4::Scheduler> scheduler);

  long op_action_create (MettEagle::Manager_Client::Rights,
                         const L4::Ipc::String_in_buf<> &_name,
                         L4::Ipc::Snd_fpage file,
                         MettEagle::Language lang);

  long op_action_delete (MettEagle::Manager_Client::Rights,
                         const L4::Ipc::String_in_buf<> &_name);
};