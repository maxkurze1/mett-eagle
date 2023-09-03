/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "app_model.h"

#include <l4/liballoc/alloc>
#include <l4/re/error_helper>
#include <l4/re/util/env_ns>

#include <l4/liblog/log>

using L4Re::chkcap;
using L4Re::chksys;

/**
 * @brief Allocate a Dataspace
 *
 * This method allocates a Dataspace capability and then
 * uses the mem_alloc to reserve 'size' bytes of memory for it.
 *
 * @param size  Needed memory in bytes
 * @return App_model::Dataspace  The dataspace object representing the memory
 */
App_model::Dataspace
App_model::alloc_ds (unsigned long size) const
{
  Dataspace mem = Dataspace (chkcap (
      L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> (), "allocate capability"));
  auto _ma = L4::Cap<L4Re::Mem_alloc> (prog_info ()->mem_alloc.raw & L4_FPAGE_ADDR_MASK);
  chksys (_ma->alloc (size, mem.get ()), "allocate writable program segment");
  return mem;
}

/**
 * @brief Tell the region mapper to attach the dataspace to a specific region
 *
 * @param addr   Address to attach to
 * @param size   The size of the dataspace
 * @param ds     The dataspace object
 * @param offset
 * @param flags
 * @param what   Failure message
 */
void
App_model::prog_attach_ds (l4_addr_t addr, unsigned long size,
                           Const_dataspace ds, unsigned long offset,
                           L4Re::Rm::Flags flags, char const *what)
{
  auto rh_flags = flags;

  if (!ds.is_valid ())
    rh_flags |= L4Re::Rm::F::Reserved;

  l4_addr_t _addr = addr;
  L4Re::chksys (
      _rm->attach (&_addr, size, rh_flags,
                   L4::Ipc::make_cap (ds.get (), flags.cap_rights ()), offset,
                   0),
      what);
}

int
App_model::prog_reserve_area (l4_addr_t *start, unsigned long size,
                              L4Re::Rm::Flags flags, unsigned char align)
{
  return _rm->reserve_area (start, size, flags, align);
}

void
App_model::copy_ds (Dataspace dst, unsigned long dst_offs, Const_dataspace src,
                    unsigned long src_offs, unsigned long size)
{
  L4Re::chksys (dst->copy_in (dst_offs, src.get (), src_offs, size),
                "copy failed");
}

l4_addr_t
App_model::local_attach_ds (Const_dataspace ds, unsigned long size,
                            unsigned long offset) const
{
  auto rm = L4Re::Env::env ()->rm ();
  l4_addr_t pg_offset = l4_trunc_page (offset);
  l4_addr_t in_pg_offset = offset - pg_offset;
  unsigned long pg_size = l4_round_page (size + in_pg_offset);
  l4_addr_t vaddr = 0;
  chksys (rm->attach (&vaddr, pg_size,
                      L4Re::Rm::F::Search_addr | L4Re::Rm::F::R, ds.get (),
                      pg_offset),
          "attach temporary VMA");
  return vaddr + in_pg_offset;
}

void
App_model::local_detach_ds (l4_addr_t addr, unsigned long /*size*/) const
{
  auto rm = L4Re::Env::env ()->rm ();
  l4_addr_t pg_addr = l4_trunc_page (addr);
  chksys (rm->detach (pg_addr, 0), "detach temporary VMA");
}

App_model::App_model (L4Re::Util::Shared_cap<L4Re::Parent> const &parent,
                      L4Re::Util::Shared_cap<L4::Scheduler> const &scheduler,
                      L4Re::Util::Shared_cap<L4::Factory> const &alloc)
    : _task (L4Re::chkcap (L4Re::Util::make_unique_cap<L4::Task> (),
                           "allocating task cap")),
      _thread (L4Re::chkcap (L4Re::Util::make_unique_cap<L4::Thread> (),
                             "allocating thread cap")),
      _rm (L4Re::chkcap (L4Re::Util::make_unique_cap<L4Re::Rm> (),
                         "allocating region-map cap"))
{
  L4Re::chksys (alloc->create (_rm.get ()), "allocating new region map");

  // set default values for utcb area, values may be changed by loader
  prog_info ()->utcbs_start = Utcb_area_start;
  prog_info ()->utcbs_log2size = L4_PAGESHIFT;

  // set default values for the application stack
  extern char __L4_KIP_ADDR__[];

  // clang-format off
  prog_info ()->kip       = (l4_addr_t)__L4_KIP_ADDR__;

  prog_info ()->rm        = _rm.fpage ();
  prog_info ()->parent    = parent.fpage ();

  prog_info ()->mem_alloc = alloc.fpage ();
  prog_info ()->log       = L4Re::Env::env ()->log ().fpage ();
  prog_info ()->factory   = L4Re::Env::env ()->factory ().fpage ();
  prog_info ()->scheduler = scheduler.fpage ();
  prog_info ()->ldr_flags = 0; // TODO improve performance with COW??
  prog_info ()->l4re_dbg  = 0;
  // clang-format on
}

/**
 * Called by the Loader (from Loader#launch) to create the stack
 */
App_model::Dataspace
App_model::alloc_app_stack ()
{
  // create a new kernel-object for the Dataspace of the stack
  auto stack = chkcap (L4Re::Util::make_shared_cap<L4Re::Dataspace> (),
                       "allocate stack capability");
  // get the memory allocator of the new process
  auto ma =
      L4::Cap<L4Re::Mem_alloc> (prog_info ()->mem_alloc.raw & L4_FPAGE_ADDR_MASK);

  // allocate the needed memory
  chksys (ma->alloc (_stack.stack_size (), stack.get ()), "allocate stack");

  // map the allocated memory to the virtual address space of the
  // new process and adjust the stack pointer
  _stack.set_stack (stack, _stack.stack_size ());

  return stack;
}

/**
 * @brief Return a factory and the (empty) capabilities of main task and thread
 *
 * The factory will be used to create the task and the thread kernel objects
 *
 * @param factory  A factory to create new kernel objects
 * @param task     The task that wraps the whole process
 * @param thread   The thread that will execute the main
 */
void
App_model::get_task_caps (L4::Cap<L4::Factory> *factory,
                          L4::Cap<L4::Task> *task, L4::Cap<L4::Thread> *thread)
{
  *task = _task.get ();
  *thread = _thread.get ();
  *factory = L4::Cap<L4::Factory> (prog_info ()->factory.raw & L4_FPAGE_ADDR_MASK);
}
