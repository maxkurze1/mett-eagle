#include "exec.h"
#include <l4/cxx/ref_ptr>
#include <l4/libloader/elf>

int
execve (const char *pathname, const char *const argv[],
        const char *const envp[], const Cap *const capabilities[])
{
  Am am;
  am.set_argv_strings (argv);
  am.set_envp_strings (envp);
  am.set_capabilities (capabilities);

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

  printf ("2pathname: %s\n", pathname);
  // App_ptr ... ==========================================

  cxx::Ref_ptr<App_task> app_task (
      new App_task (server.registry (), am.rm_fab ()));

  printf ("3pathname: %s\n", pathname);
  // am#set_task

  am._task = app_task.get ();
  app_task->running ();
  printf ("4pathname: %s\n", pathname);

  // am#launch_loader

  typedef Ldr::Elf_loader<Am, Dbg> Loader;

  Dbg dbg (Dbg::Loader, "ldr");
  Loader ldr;
  printf ("5pathname: %s\n", pathname);
  ldr.launch (&am, pathname, dbg);

  return 0;
}