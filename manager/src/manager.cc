#include <l4/cxx/ref_ptr>
#include <l4/libloader/elf>
#include <l4/libloader/loader>
#include <l4/libloader/remote_app_model>
#include <l4/libloader/remote_mem>
#include <l4/libloader/stack>
#include <l4/ned/cmd_control>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/debug>
#include <l4/re/util/env_ns>
#include <stdio.h>
#include <unistd.h>

#include "app_model.h"
#include "app_task.h"
#include "debug.h"
#include "server.h"

// Ldr::Elf_loader<
//     Ldr::Remote_app_model<Ldr::Base_app_model<Ldr::Remote_stack<> > >,
//     L4Re::Util::Dbg>
//     loader;
// some magic mask
// L4Re::Util::Dbg dbg (0x80, "mett-eagle", "ldr");

class Am : public Ldr::Remote_app_model<App_model>
{
public:
  L4Re::Util::Ref_cap<L4::Factory>::Cap _rm_fab;

  L4Re::Util::Ref_cap<L4::Factory>::Cap
  rm_fab () const
  {
    return _rm_fab;
  }

  l4_cap_idx_t
  push_initial_caps (l4_cap_idx_t start)
  {

    // char const *r = "some_cap";
    // if (!l4re_env_cap_entry_t::is_valid_name (r))
    // error? (_lua, "Capability name '%s' too long", r);
    // _stack.push (l4re_env_cap_entry_t (r, get_initial_cap (r, &start)));
    return start;
  }

  void
  map_initial_caps (L4::Cap<L4::Task> task, l4_cap_idx_t start)
  {
    char const *r = "some_cap";
    // get capability??
    // Cap *c = idk
    // auto idx = get_initial_cap (r, &start);
    // chksys (task->map (L4Re::This_task, c->cap<void> ().fpage (c->rights
    // ()),
    //  L4::Cap<void> (idx).snd_base () | c->ext_rights ()));
  }
  void
  push_argv_strings ()
  {
    // argv.a0 = 0;
    // for (int i = _arg_idx; i <= _argc; ++i)
    //   {
    //     size_t l;
    //     char const *r = ""; // get string and size from lua??
    //     argv.al = _stack.push_str (r, l);
    //     if (argv.a0 == 0)
    //       argv.a0 = argv.al;
    //   }
    argv.al = _stack.push_str ("worker", 6);
    argv.a0 = argv.al;
  }

  void
  push_env_strings ()
  {
    //   if (!_env_idx)
    //     return;

    //   bool _f = true;
    //   while (lua_next (_lua, _env_idx))
    //     {
    //       size_t kl;
    //       char const *k = luaL_checklstring (_lua, -2, &kl);
    //       size_t vl;
    //       char const *v = luaL_checklstring (_lua, -1, &vl);

    //       _stack.push_str (v, vl);
    //       _stack.push ('=');
    //       envp.al = _stack.push_object (k, kl);
    //       if (_f)
    //         {
    //           envp.a0 = envp.al;
    //           _f = false;
    //         }
    //     }
    // }
  }
};

static Mett_Eagle::Server s;
static Dbg info (Dbg::Info);
Mett_Eagle::Server *Mett_Eagle::server = &s;
l4re_aux_t *l4re_aux;

int
exec ()
{
  Am am;

  // am#parse_cfg ===========================================

  L4Re::Util::Ref_cap<L4::Factory>::Cap user_factory
      = L4Re::Env::env ()->user_factory ();

  am.prog_info ()->mem_alloc = user_factory.fpage ();
  am.prog_info ()->log = L4Re::Env::env ()->log ().fpage ();
  am.prog_info ()->factory = L4Re::Env::env ()->factory ().fpage ();
  am.prog_info ()->scheduler = L4Re::Env::env ()->scheduler ().fpage ();
  //  parser.scheduler_cap.set_fpage(&am.prog_info()->scheduler);

  am.prog_info ()->ldr_flags = 0;
  am.prog_info ()->l4re_dbg = 0;

  // prog_info ()->ldr_flags
  //     = _cfg_integer ("ldr_flags", prog_info ()->ldr_flags);
  // prog_info ()->l4re_dbg = _cfg_integer ("l4re_dbg", prog_info
  // ()->l4re_dbg);

  // _cap_stack.push (_cfg_cap<void> ("log", &prog_info ()->log));
  // _cap_stack.push (_cfg_cap<void> ("mem", &prog_info ()->mem_alloc));
  // _cap_stack.push (_cfg_cap<void> ("factory", &prog_info ()->factory));
  // _cap_stack.push (_cfg_cap<void> ("scheduler", &prog_info ()->scheduler));

  // auto c = _cfg_cap<L4::Factory> ("rm_fab");
  // if (c)
  //   _rm_fab = c;
  // else
  am._rm_fab = user_factory;

  // App_ptr ... ==========================================

  info.printf ("creating task\n");

  cxx::Ref_ptr<App_task> app_task (
      new App_task (Mett_Eagle::server->registry (), am.rm_fab ()));

  // am#set_task

  am._task = app_task.get ();
  app_task->running ();

  // am#launch_loader

  info.printf ("launching\n");

  char const *kernel = "rom/worker";
  typedef Ldr::Elf_loader<Am, Dbg> Loader;

  Dbg ldr (Dbg::Loader, "ldr");
  Loader _l;

  _l.launch (&am, kernel, ldr);

  return 1;
}

int
main (int argc, char *const argv[])
{
  // (void)argc;
  // (void)argv;
  info.printf ("-- Mett Eagle --\n");

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

  exec ();
  // L4Re::Util::Dbg dbg (/* Dbg::Loader */ 0x80, "mett-eagle", "ldr");
  // Ldr::Elf_loader<
  //     Ldr::Remote_app_model<Ldr::Base_app_model<Ldr::Remote_stack<> > >,
  //     L4Re::Util::Dbg>
  //     loader;

  // Ldr::Remote_app_model<Ldr::Base_app_model<Ldr::Remote_stack<> > > am;
  // loader.launch (&am, "rom/worker", dbg);

  // Region_map rm;
  // L4Re::Env::rm()
  // L4::Cap<L4Re::Dataspace> file = L4Re_app_model::open_file
  // ("rom/worker"); loader.launch (file, L4Re::Env::env ()->rm ());
  // loader.launch (&am, "rom/worker", dbg);

  // L4Re::Env const *e = L4Re::Env::env ();
  // // e->mem_alloc()
  // // e->rm()
  // auto ned = L4Re::chkcap (
  //     L4Re::Env::env ()->get_cap<L4Re::Ned::Cmd_control> ("ned"),
  //     "Couldn't find ned!");
  // L4::Ipc::String<> s (
  //     "local L4 = require(\"L4\");L4.default_loader:start({}, \"rom/worker
  //     " "some_arg\");");
  // ned->execute (s);

  // for (int i = 0; i < 10; i++)
  //   {
  //     ned->execute (s);
  //   }
}