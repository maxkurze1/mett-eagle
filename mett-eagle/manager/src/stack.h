/*
 * (c) 2008-2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
#pragma once

#include "manager.h"

#include <l4/libloader/remote_mem>
#include <l4/re/dataspace>
#include <l4/re/error_helper>
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>
#include <l4/sys/capability>
#include <l4/sys/types.h>

#include <l4/re/util/shared_cap>

class Stack_base
{
public:
  typedef L4Re::Util::Shared_cap<L4Re::Dataspace> Dataspace;

protected:  
  Dataspace _stack_ds;
  L4Re::Rm::Unique_region<char *> _vma;

  l4_addr_t _last_checked;

  Stack_base () : _last_checked (0) {}

  void
  check_access (char *addr, size_t sz)
  {
    if (_last_checked != l4_trunc_page (l4_addr_t (addr)))
      {
        l4_addr_t offs = l4_trunc_page (addr - _vma.get ());
        l4_addr_t end = l4_round_page (addr + sz - _vma.get ());
        chksys (_stack_ds->allocate (offs, end - offs));
        _last_checked = l4_trunc_page (l4_addr_t (addr));
      }
  }
};

class Stack : public Ldr::Remote_stack<Stack_base>
{
public:
  explicit Stack () : Ldr::Remote_stack<Stack_base> (0) {}
  void set_stack (Stack_base::Dataspace const &ds, unsigned size);
};
