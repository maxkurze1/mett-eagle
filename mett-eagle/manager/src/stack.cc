/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "stack.h"

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/rm>
#include <l4/re/util/shared_cap>

void
Stack::set_stack (L4Re::Util::Shared_cap<L4Re::Dataspace> const &ds,
                  unsigned size)
{
  chksys (L4Re::Env::env ()->rm ()->attach (
                    &_vma, size, L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                    L4::Ipc::make_cap_rw (ds.get ()), 0),
                "attaching stack vma");
  _stack_ds = ds;
  set_local_top ((char *)(_vma.get () + size));
}
