/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
/**
 * @file
 * Epiface declaration for the worker interface
 *
 * @see <l4/mett-eagle/worker>
 */

#pragma once

#include "manager.h"
#include "manager_base.h"
#include "worker.h"

#include <l4/mett-eagle/worker>

#include <l4/re/util/shared_cap>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/scheduler>
#include <l4/sys/thread>

#include <chrono>

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

  /* end will be measured as short as possible after worker exit */
  /* this will be measured in the corresponding rpc handler */
  std::chrono::time_point<std::chrono::high_resolution_clock> end;

public:
  Manager_Worker_Epiface (
      std::shared_ptr<std::map<std::string, Action> > actions,
      L4::Cap<L4::Thread> thread,
      L4Re::Util::Shared_cap<L4::Scheduler> scheduler,
      std::shared_ptr<Worker> worker);

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
                const L4::Ipc::String_in_buf<> &_value);
};
