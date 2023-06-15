/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "manager.h"
#include "app_model.h"
#include "worker.h"

#include <list>
#include <map>
#include <string>

#include <l4/liblog/log>
#include <l4/mett-eagle/client>
#include <l4/mett-eagle/manager>

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/l4aux.h>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/env_ns>
#include <l4/re/util/object_registry>

/**
 * Absolutely no clue what this does..
 * Something with the kip ...
 * KIP === Kernel Interface Page
 */
l4re_aux_t *l4re_aux;

/**
 * This is the object will handle all RPC calls and forward them
 * to the corresponding interface implementations.
 *
 * It will handle RPC's from clients as well as RPC's from started
 * processes (= workers).
 *
 * Br_manager is necessary to handle the demand of cap slots. For example:
 * register_client will use a cap slot fot the client ipc_gate passed to the
 * method.
 */
L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

/**
 * This will map the ipc gate which received a message to the client ipc gate
 * which will receive the answer.
 *
 * @note Per default the kernel 'ipc reply' will be used to answer calls, but
 * in cases where the processing of the message might take very long (e.g.
 * invoking a serverless function) the rpc call will return early and the
 * clients ipc gate will be used to answer.
 */
std::map<L4::Cap<MettEagle::Manager_Base>, L4::Cap<MettEagle::Client> >
    gate_to_client;

std::list<Worker *> workers;

long
Manager_Base_Epiface::op_action_invoke (MettEagle::Manager_Base::Rights,
                                        const L4::Ipc::String_in_buf<> &name)
{
  log_info ("Invoke action name='%s'", name.data);
  /* c++ maps dont have a map#contains */
  if (_actions->count (name.data) == 0)
    {
      log_error ("Action %s doensn't exists", name.data);
      return -L4_EINVAL;
    }

  L4::Cap<L4Re::Dataspace> file = (*_actions)[name.data];
  /* cap can be unmapped anytime ... add try catch or some error handling */
  if (not file.is_valid ())
    log_error ("dataspace invalid");

  /* create new ipc_gate for started process */
  auto worker_epiface = new Manager_Worker_Epiface (_actions);

  /* register the object in the server loop. This will create the        *
   * capability for the object and inform the server to route IPC there. */
  L4::Cap<void> cap = server.registry ()->register_obj (worker_epiface);
  if (L4_UNLIKELY (not cap.is_valid ()))
    {
      log_error ("Failed to register IPC gate");
      return -L4_ENOMEM;
    }

  /* associate new worker_epiface with client gate to answer */
  gate_to_client[worker_epiface->obj_cap ()] = gate_to_client[obj_cap ()];

  /* Pass the IPC gate to process as parent */
  Worker *worker = new Worker (file, worker_epiface->obj_cap ());
  worker->set_argv_strings ({ name.data });
  worker->set_envp_strings ({ "PKGNAME=Worker    ", "LOG_LEVEL=DEBUG" });
  worker->launch ();
  workers.push_back (worker);

  log_info ("Started process");
  return L4_EOK;
}

long
Manager_Client_Epiface::op_action_create (MettEagle::Manager_Client::Rights,
                                          const L4::Ipc::String_in_buf<> &name,
                                          L4::Ipc::Snd_fpage file)
{
  log_debug ("Create action name='%s' file passed='%d'", name.data,
             file.cap_received ());
  if (L4_UNLIKELY (not file.cap_received ()))
    {
      log_error ("No capability received");
      return -L4_EINVAL;
    }

  /* get the received capability */
  (*_actions)[name.data] = server_iface ()->rcv_cap<rec (0) < 0))
    {
      log_error ("Failed to realloc_rcv_cap");
      return -L4_ENOMEM;
    }
  return L4_EOK;
}
long
Manager_Worker_Epiface::op_signal (L4Re::Parent::Rights, unsigned long sig,
                                   unsigned long value)
{
  if (sig == 0) /*  exit -- why not SIGCHLD ? */
    {
      /* faas function probably threw an error   *
       * in this case value holds the error code */
      log_error ("Worker finished with wrong exit!");
      // TODO return error to parent

      if (not gate_to_client.count (obj_cap ()))
        {
          log_error ("Couldn't find client of worker!");
        }
      else
        {
          auto client = gate_to_client[obj_cap ()];
          // TODO check client cap valid
          client->answer ("worker crashed");
        }

      // terminate ();

      //     delete this;

      /* do not send answer -- child shouldn't exist anymore */
      return -L4_ENOREPLY;
    }
  /* do nothing per default */
  return L4_EOK;
}
long
Manager_Worker_Epiface::op_exit (MettEagle::Manager_Worker::Rights,
                                 const L4::Ipc::String_in_buf<> &value)
{
  log_debug ("Worker exit");
  if (not gate_to_client.count (obj_cap ()))
    {
      log_error ("Couldn't find client of worker!");
    }
  else
    {
      auto client = gate_to_client[obj_cap ()];
      // TODO check client cap valid
      client->answer (value.data);
    }
  // terminate ();

  //     delete this;

  /* do not send answer -- block worker */

  /* With -L4_ENOREPLY no answer will be send to the worker. Thus the worker *
   * will keep waiting.                                                      */
  return -L4_ENOREPLY;
}

long
Manager_Registry_Epiface::op_register_client (
    MettEagle::Manager_Registry::Rights, L4::Ipc::Snd_fpage client_ipc_gate,
    L4::Ipc::Cap<MettEagle::Manager_Client> &manager_ipc_gate)
{
  log_debug ("Registering client");
  if (L4_UNLIKELY (not client_ipc_gate.cap_received ()))
    {
      log_error ("No capability received");
      return -L4_EINVAL;
    }
  /* get the received capability */
  auto client = server_iface ()->rcv_cap<MettEagle::Client> (0);
  /* allocate new capability slot for next client */
  if (L4_UNLIKELY (server_iface ()->realloc_rcv_cap (0) < 0))
    {
      log_error ("Failed to realloc_rcv_cap");
      return -L4_ENOMEM;
    }

  /* create new object handling the requests of this client */
  auto epiface = new Manager_Client_Epiface ();
  /*
   * register the object in the server loop. This will create the
   * capability for the object and inform the server to route IPC there.
   */
  L4::Cap<void> cap = server.registry ()->register_obj (epiface);
  if (L4_UNLIKELY (not cap.is_valid ()))
    {
      log_error ("Failed to register client IPC gate");
      return -L4_ENOMEM;
    }

  /* associate client ipc gate with epiface */
  gate_to_client[epiface->obj_cap ()] = client;

  /* Pass the IPC gate back to the client as output argument. */
  manager_ipc_gate = epiface->obj_cap ();
  return L4_EOK;
}

/**
 * @brief Global manager registry handler
 *
 * This object will handle the registration of all new clients.
 */
Manager_Registry_Epiface metteagle_registry_epiface;

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

    /*
     * Associate the 'server' endpoint that was already 'reserved' by ned
     * with the newly created interface implementation
     */
    L4Re::chkcap (
        server.registry ()->register_obj (&metteagle_registry_epiface,
                                          "server"),
        "Couldn't register service, is there a 'server' in the caps table?");

    log_info ("Started Mett-Eagle server!");

    // start server loop -- loop will not return!
    server.loop ();
  }
catch (L4::Runtime_error &e)
  {
    /**
     * Catch all errors (e.g. from chkcap) and log some message
     */
    log_fatal (e);
    return EXIT_FAILURE;
  }