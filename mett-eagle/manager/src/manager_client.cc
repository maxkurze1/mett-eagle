/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */

#include "manager_client.h"

Manager_Client_Epiface::Manager_Client_Epiface (
    L4::Cap<L4::Thread> thread,
    L4Re::Util::Shared_cap<L4::Scheduler> scheduler)
{
  /* _actions map will be create by the clients epiface and only *
   * passed to each worker epiface.                              */
  _actions = std::make_shared<std::map<std::string, Action> > ();

  _thread = thread;
  _scheduler = scheduler;
}

long
Manager_Client_Epiface::op_action_create (
    MettEagle::Manager_Client::Rights, const L4::Ipc::String_in_buf<> &_name,
    L4::Ipc::Snd_fpage file, MettEagle::Language lang)
{
  const char *name = _name.data;
  // log<DEBUG> ("Create action name='{:s}' file passed='{}'", name,
              // file.cap_received ());

  if (L4_UNLIKELY (not file.cap_received ()))
    throw Loggable_exception (-L4_EINVAL, "No dataspace cap received");
  auto cap = server_iface ()->rcv_cap<L4Re::Dataspace> (0);

  if (L4_UNLIKELY (not cap.validate ().label ()))
    throw Loggable_exception (-L4_EINVAL, "Received capability is invalid");
  if (L4_UNLIKELY (_actions->count (name) != 0))
    throw Loggable_exception (-L4_EEXIST, "Action '{:s}' already exists",
                              name);

  /* safe the received capability and language*/
  (*_actions)[name] = { L4Re::Util::Shared_cap<L4Re::Dataspace> (cap), lang };
  if (L4_UNLIKELY (server_iface ()->realloc_rcv_cap (0) < 0))
    throw Loggable_exception (-L4_ENOMEM, "Failed to realloc_rcv_cap");

  return L4_EOK;
}

long
Manager_Client_Epiface::op_action_delete (
    MettEagle::Manager_Client::Rights, const L4::Ipc::String_in_buf<> &_name)
{
  const char *name = _name.data;
  // log<DEBUG> ("Deleting action name='{:s}'", name);

  /* remove the dataspace from the map */
  /* this should decrease the ref count and unmap the dataspace in case no
   * worker is currently using it */
  _actions->erase (name);

  return L4_EOK;
}