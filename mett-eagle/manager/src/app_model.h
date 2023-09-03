/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/l4aux.h>

#include "stack.h"
#include <l4/libloader/elf>
#include <l4/libloader/loader>
#include <l4/libloader/remote_app_model>
#include <l4/liblog/log>
#include <l4/re/util/debug>
#include <l4/re/util/shared_cap>
#include <l4/re/util/unique_cap>

#include <l4/liblog/loggable-exception>

/**
 * @brief This class will provide all functions that are needed but not
 * implemented by Base_app_model/Loader and Remote_app_model
 */
struct App_model : public Ldr::Base_app_model<Stack>
{

  enum // TODO
  {
#ifdef ARCH_mips
    Utcb_area_start = 0x73000000, // this needs to be lower on MIPS
#else
    Utcb_area_start = 0xb3000000,
#endif
  };

  // necessary for Base_app_model
  /**
   * the passed binary of the client will be handled as Const_dataspace
   * therefore Const_dataspace's should not be deleted
   */
  typedef L4Re::Util::Shared_cap<L4Re::Dataspace> Const_dataspace;
  typedef Stack_base::Dataspace Dataspace;

  L4Re::Util::Unique_cap<L4::Task> _task;
  L4Re::Util::Unique_cap<L4::Thread> _thread;
  L4Re::Util::Unique_cap<L4Re::Rm> _rm;

  explicit App_model (L4Re::Util::Shared_cap<L4Re::Parent> const &parent,
                      L4Re::Util::Shared_cap<L4::Scheduler> const &scheduler,
                      L4Re::Util::Shared_cap<L4::Factory> const &alloc);

  Dataspace alloc_ds (unsigned long size) const;

  /* this function is not used -- all binaries are passed as Const_dataspace */
  Const_dataspace
  open_file (char const *)
  {
    throw L4Re::LibLog::Loggable_exception (-L4_EINVAL,
                                            "open_file is not implemented");
  }

  /* needed by Remote_app_model */
  virtual l4_cap_idx_t push_initial_caps (l4_cap_idx_t start) = 0;
  /* needed by Remote_app_model */
  virtual void map_initial_caps (L4::Cap<L4::Task> task, l4_cap_idx_t start)
      = 0;

  /* needed by Remote_app_model, Elf Loader */
  void prog_attach_ds (l4_addr_t addr, unsigned long size, Const_dataspace ds,
                       unsigned long offset, L4Re::Rm::Flags flags,
                       char const *what);

  static void copy_ds (Dataspace dst, unsigned long dst_offs,
                       Const_dataspace src, unsigned long src_offs,
                       unsigned long size);

  bool
  all_segs_cow ()
  {
    return false;
  }

  l4_addr_t local_attach_ds (Const_dataspace ds, unsigned long size,
                             unsigned long offset) const;

  void local_detach_ds (l4_addr_t addr, unsigned long size) const;

  int prog_reserve_area (l4_addr_t *start, unsigned long size,
                         L4Re::Rm::Flags flags, unsigned char align);

  Dataspace alloc_app_stack ();

  /**
   * Called by the Loader (from Loader#launch) after creation of the stack.
   *
   * Thus this function should put argv and envp on the stack.
   */
  void
  init_prog ()
  {
    push_argv_strings ();
    push_env_strings ();
  }

  static Const_dataspace
  reserved_area ()
  {
    return Const_dataspace ();
  }

  static Dataspace
  local_kip_ds ()
  {
    extern l4re_aux_t *l4re_aux;
    // works because kip_ds is not managed by the Util::cap_alloc and therefore
    // the ref count is not decreased -- Shared_cap is doing nothing here
    return Dataspace (L4::Cap<L4Re::Dataspace> (l4re_aux->kip_ds));
  }

  static L4::Cap<void>
  local_kip_cap ()
  {
    return local_kip_ds ().get ();
  }

  void get_task_caps (L4::Cap<L4::Factory> *factory, L4::Cap<L4::Task> *task,
                      L4::Cap<L4::Thread> *thread);

  l4_msgtag_t
  run_thread (L4::Cap<L4::Thread> thread, l4_sched_param_t const &)
  {
    auto scheduler = L4::Cap<L4::Scheduler> (prog_info ()->scheduler.raw
                                             & (~0UL << L4_FPAGE_ADDR_SHIFT));

    l4_umword_t cpu_max;
    l4_sched_cpu_set_t cpus = l4_sched_cpu_set (0, 0);
    l4_msgtag_t t = scheduler->info (&cpu_max, &cpus);
    if (L4_UNLIKELY(l4_error (t)))
      return t;

    l4_sched_param_t sp = l4_sched_param (L4_SCHED_MIN_PRIO);
    sp.affinity = cpus;

    using namespace L4Re::LibLog;
    log<DEBUG> ("Scheduling on cpu {:#x}", cpus.map);

    return scheduler->run_thread (thread, sp);
  }

  virtual void push_argv_strings () = 0;
  virtual void push_env_strings () = 0;

  virtual ~App_model () noexcept{};
};
