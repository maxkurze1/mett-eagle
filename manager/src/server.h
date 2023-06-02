/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <cstdio>
#include <l4/cxx/exceptions>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <pthread.h>

/**
 * This interface can be used by spawned
 * processes to communicate with the manager?
 */
class Server_object : public L4::Epiface
{
};

// class Server : public L4::Server<>
// {
// private:
//   L4Re::Util::Object_registry *_r;
//   pthread_t _th;
//   pthread_mutex_t _start_mutex;
//   static void *__run (void *);

//   void run ();

// public:
//   typedef L4::Server<> Base;

//   Server ();

//   L4Re::Util::Object_registry *
//   registry ()
//   {
//     return _r;
//   }
// };

// extern Server *server;
extern L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;