/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "app_model.h"
#include <l4/libloader/remote_app_model>
#include <l4/re/util/cap>
#include <l4/re/util/cap_alloc>
#include <list>
#include <string>

/**
 * App model that is really used in the end to start the worker process
 *
 * This class represents the new process that will be started
 * Am extends Remote_app_model extends App_model extends Base_app_model
 */
class Worker : public Ldr::Remote_app_model<App_model>
{
private:
  /**
   * This data class is used to specify
   * which capabilities are passed to the created process,
   * the names they will have, the rights that will be transferred
   * and the flags that are used for the fpage??
   */
  struct Initial_Cap
  {
    L4::Cap<void> capability;
    std::string name;
    unsigned rights = 16;
    unsigned flags = 16;
  };

  std::list<std::string> _argv;
  std::list<std::string> _envp;
  std::list<Initial_Cap> _initial_capabilities;

  bool _alive = true;
  std::string _exit_value;

  Const_dataspace _bin;

public:
  explicit Worker (Const_dataspace bin,
                   L4Re::Util::Ref_del_cap<L4Re::Parent>::Cap const &parent,
                   L4::Cap<L4::Factory> const &alloc
                   = L4Re::Env::env ()->user_factory ())
      : Remote_app_model (parent, alloc), _bin (bin)
  {
  }

  /**
   * @brief Terminate process represented by object
   *
   * All capabilities should be unmapped
   *
   * @param value  This string should hold the exit value of the process
   */
  void
  exit (std::string value)
  {
    // TODO free resources
    _exit_value = value;
    _alive = false;
  }

  /**
   * @brief Get the exit value
   */
  std::string
  get_exit_value () const
  {
    return _exit_value;
  }

  /**
   * Used to check if the worker process already sent the exit ipc
   */
  bool
  alive () const
  {
    return _alive;
  }

  /**
   * Region mapper fabric??
   */
  // L4Re::Util::Ref_cap<L4::Factory>::Cap _rm_fab;

  // L4Re::Util::Ref_cap<L4::Factory>::Cap
  // rm_fab () const
  // {
  //   return _rm_fab;
  // }

  // void
  // Worker::terminate ()
  // {
  //   _task.reset ();
  //   _thread.reset ();
  //   _rm.reset ();

  //   _r->unregister_obj (this);
  // }

  /**
   * Pushing the names of the initial capabilities to the stack ??
   * only for naming purposes ??
   *
   * also to reserve enough space / indices ??
   */
  l4_cap_idx_t
  push_initial_caps (l4_cap_idx_t start)
  {
    for (auto init_cap : _initial_capabilities)
      {
        auto name = init_cap.name.c_str ();
        if (not l4re_env_cap_entry_t::is_valid_name (name))
          {
            Log::error ("Capability name '%s' too long", name);
            continue;
          }
        _stack.push (
            l4re_env_cap_entry_t (name, get_initial_cap (name, &start)));
      }
    return start;
  }

  /**
   * This method transfers the selected capabilities of the current process
   * to the new process (task) which capabilities are transferred is configured
   * with TODO add_capability()
   */
  void
  map_initial_caps (L4::Cap<L4::Task> task, l4_cap_idx_t start)
  {
    for (auto init_cap : _initial_capabilities)
      {
        L4Re::chksys (task->map (
            L4Re::This_task, init_cap.capability.fpage (init_cap.rights),
            L4::Cap<void> (get_initial_cap (init_cap.name.c_str (), &start))
                .snd_base () /* | external rights ???*/));
      }
  }

  /**
   * Preparing the arguments that the process will receive
   *
   * The arguments must be pushed on the stack of the program,
   * because the loader expects them to be in reverse order and
   * they should make up a continuos region of memory.
   *
   * argv.al should point to the last string and
   * argv.a0 should point to the first string
   *
   * a0 == 0 means there are no arguments
   */
  void
  push_argv_strings ()
  {
    if (_argv.empty ())
      return;

    auto iter = _argv.begin ();
    auto first = iter->c_str ();

    // special handling of first to set a0
    // push_str returns the start address of the string on the stack
    argv.al = _stack.push_str (first, strlen (first));
    argv.a0 = argv.al;

    for (iter++; iter != _argv.end (); iter++)
      argv.al = _stack.push_str (iter->c_str (), iter->length ());
  }

  /**
   * Add an argument value that is accessible to the
   * main of the executed program.
   *
   * Example:
   * @code{.cpp}
   * add_argv_string ("hello");
   * @endcode
   */
  void
  add_argv_string (std::string argv)
  {
    _argv.push_back (argv);
  }

  /**
   * Set the argument values that are accessible to the
   * main of the executed program.
   *
   * Example:
   * @code{.cpp}
   * const char *third_param  = "third";
   * set_argv_strings ({ "hello", "world", third_param });
   * @endcode
   */
  void
  set_argv_strings (std::list<std::string> argv)
  {
    _argv = argv;
  }

  /**
   * Preparing the POSIX environment that the process will receive.
   * WARNING: Do not confuse this environment with the L4Re::env !
   *
   * The environment must be pushed on the stack of the program,
   * because the loader expects it to be in reverse order and
   * it should make up a continuos region of memory.
   *
   * envp.al should point to the last string and
   * envp.a0 should point to the first string
   *
   * a0 == 0 means there are no arguments
   */
  void
  push_env_strings ()
  {
    if (_envp.empty ())
      return;

    auto iter = _envp.begin ();
    auto first = iter->c_str ();

    // special handling of first to set a0
    // push_str returns the start address of the string on the stack
    envp.al = _stack.push_str (first, strlen (first));
    envp.a0 = envp.al;

    // loop through envs and push them on the stack
    for (iter++; iter != _envp.end (); iter++)
      envp.al = _stack.push_str (iter->c_str (), iter->length ());
  }

  /**
   * Add a POSIX environment values that the program will receive.
   * WARNING: Do not confuse this environment with the L4Re::env !
   *
   * Example:
   * @code{.cpp}
   * add_envp_string ("LOG_LEVEL=info");
   * @endcode
   */
  void
  add_envp_string (std::string envp)
  {
    _envp.push_back (envp);
  }

  /**
   * Set the POSIX environment values that the program will receive.
   * WARNING: Do not confuse this environment with the L4Re::env !
   *
   * Example:
   * @code{.cpp}
   * set_envp_strings ({ "LOG_LEVEL=info", "PKGNAME=Worker" });
   * @endcode
   */
  void
  set_envp_strings (std::list<std::string> envp)
  {
    _envp = envp;
  }

  /**
   * This function can be used to configure a capability that should be
   * mapped to the new process once it will be created. It will be added to the
   * so called 'initial capabilities'
   *
   * @param cap     The capability that should be mapped
   * @param name    Name of the capability inside the new process
   * @param rights  The rights that should be mapped
   * @param flags   TODO flags?
   */
  void
  add_initial_capability (L4::Cap<void> cap, std::string name,
                          unsigned rights = 16, unsigned flags = 16)
  {
    if (not l4re_env_cap_entry_t::is_valid_name (name.c_str ()))
      {
        Log::error ("Capability name '%s' too long", name);
        return;
      }
    _initial_capabilities.push_back ({ cap, name, rights, flags });
  }

  /**
   * Creates an Ldr::Elf_loader and uses it to start this new process
   */
  void
  launch ()
  {
    // Mask of 0 will silence everything
    L4Re::Util::Dbg dbg (0, "Mett-Eagle", "ldr");
    Ldr::Elf_loader<Worker, L4Re::Util::Dbg> loader;
    loader.launch (this, _bin, dbg);
  }
};