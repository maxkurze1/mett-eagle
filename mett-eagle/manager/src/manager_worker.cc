/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "manager_worker.h"

Manager_Worker_Epiface::Manager_Worker_Epiface (
    std::shared_ptr<std::map<std::string, Action> > actions,
    L4::Cap<L4::Thread> thread,
    L4Re::Util::Shared_cap<L4::Scheduler> scheduler,
    std::shared_ptr<Worker> worker)
{
  /* passed actions map from the client */
  _actions = actions;
  _thread = thread;
  _scheduler = scheduler;
  _worker = worker;
}

long
Manager_Worker_Epiface::op_signal (L4Re::Parent::Rights, unsigned long sig,
                                   unsigned long val)
{
  if (sig == 0) /*  exit -- why not SIGCHLD ? */
    {
      this->end = std::chrono::high_resolution_clock::now ();

      auto err = static_cast<int> (val);

      /*
       * faas function probably threw an error, else op_exit should have been
       * called.
       * in case of an error, value holds the error code
       */
      log<ERROR> ("Worker finished with wrong exit! {:s} (=exit: {:d})",
                  l4sys_errtostr (err), err);

      /* using exit(int) to indicate unusual exit */
      _worker->exit (err);

      /* do not send answer -- child shouldn't exist anymore */
      return -L4_ENOREPLY;
    }
  else
    {
      log<WARN> ("Got unknown signal '{:d}' with value '{:d}'", sig, val);
    }
  /* do nothing per default */
  return L4_EOK;
}

long
Manager_Worker_Epiface::op_exit (MettEagle::Manager_Worker::Rights,
                                 const L4::Ipc::String_in_buf<> &_value)
{
  this->end = std::chrono::high_resolution_clock::now ();

  const char *value = _value.data;

  log<DEBUG> ("Worker exit: {:s}", value);
  _worker->exit (value);

  /* With -L4_ENOREPLY no answer will be send to the worker. Keep the worker
   * thread blocked until destroyed. */
  return -L4_ENOREPLY;
}
