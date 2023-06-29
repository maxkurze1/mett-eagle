/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/cxx/exceptions>
#include <l4/liblog/log>
#include <l4/sys/cxx/ipc_server_loop>

/**
 * @brief Error catching and logging dispatcher
 *
 * This class can be used as dispatcher for an L4::Server loop.
 * It will catch errors like L4::Ipc::svr::Exc_dispatch and log
 * them using the liblog.
 */
template <typename R, typename Exc = L4::Runtime_error>
struct Exc_log_dispatch : private L4::Ipc_svr::Direct_dispatch<R>
{
  Exc_log_dispatch (R r) : L4::Ipc_svr::Direct_dispatch<R> (r) {}

  /**
   * Dispatch the call via Direct_dispatch<R>() and handle
   * and log exceptions.
   */
  l4_msgtag_t
  operator() (l4_msgtag_t tag, l4_umword_t obj, l4_utcb_t *utcb)
  {
    try
      {
        return L4::Ipc_svr::Direct_dispatch<R>::operator() (tag, obj, utcb);
      }
    catch (Exc &e)
      {
        /* log the error */
        log_error (e);
        return l4_msgtag (e.err_no (), 0, 0, 0);
      }
    catch (long err)
      {
        log_error (l4sys_errtostr (err));
        return l4_msgtag (err, 0, 0, 0);
      }
  }
};