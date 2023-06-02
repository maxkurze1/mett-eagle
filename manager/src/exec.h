#pragma once

#include "app_model.h"
#include "debug.h"
#include <cstring>
#include <l4/cxx/avl_map>
#include <l4/cxx/string>
#include <l4/libloader/remote_app_model>
#include <l4/re/util/cap_alloc>

/**
 * This data class is used by execve to specify
 * which capabilities are passed to the created process,
 * the names they will have, the rights that will be transferred
 * and the flags that are used for the fpage??
 */
struct Cap
{
  L4::Cap<void> _capability;
  unsigned _rights : 16;
  unsigned _flags : 16;
  const char *_name;
};

/**
 * App model that is really used in the end to start the worker process
 *
 * This class represents the new process that will be started
 */
class Am : public Ldr::Remote_app_model<App_model>
{
private:
  const char *const *_argv = NULL;
  const char *const *_envp = NULL;
  const Cap *const *_capabilities = NULL;

public:
  /**
   * Region mapper fabric??
   */
  L4Re::Util::Ref_cap<L4::Factory>::Cap _rm_fab;

  L4Re::Util::Ref_cap<L4::Factory>::Cap
  rm_fab () const
  {
    return _rm_fab;
  }

  /**
   * Pushing the names of the initial capabilities to the stack ??
   * only for naming purposes ??
   *
   * also to reserve enough space / indices ??
   */
  l4_cap_idx_t
  push_initial_caps (l4_cap_idx_t start)
  {
    if (_capabilities == NULL)
      return start;
    for (const Cap *const *cap = _capabilities; *cap != NULL; cap++)
      {
        const char *const cap_name = (*cap)->_name;
        if (not l4re_env_cap_entry_t::is_valid_name (cap_name))
          // TODO proper error handling
          dbg.printf ("Capability name '%s' too long", cap_name);
        _stack.push (l4re_env_cap_entry_t (
            cap_name, get_initial_cap (cap_name, &start)));
      }

    return start;
  }

  /**
   * This method transfers the selected capabilities of the current process
   * to the new process (task) which capabilities are transferred is configured
   * with set_capabilities()
   */
  void
  map_initial_caps (L4::Cap<L4::Task> task, l4_cap_idx_t start)
  {
    if (_capabilities == NULL)
      return;
    for (const Cap *const *cap = _capabilities; *cap != NULL; cap++)
      {
        L4Re::chksys (
            task->map (L4Re::This_task,
                       (*cap)->_capability.fpage (/* TODO specify rights*/),
                       L4::Cap<void> (get_initial_cap ((*cap)->_name, &start))
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
    // make sure the argument list exists and is not empty
    if (_argv == NULL || *_argv == NULL)
      return;

    // push_str returns the start address of the string on the stack
    argv.al = _stack.push_str (*_argv, strlen (*_argv));
    argv.a0 = argv.al;

    // loop through args and push them on the stack
    for (const char *const *arg = _argv + 1; *arg != NULL; arg++)
      argv.al = _stack.push_str (*arg, strlen (*arg));
  }

  /**
   * Set the argument values that are accessible to the
   * main of the executed program.
   *
   * @param _argv This array of strings needs to be terminated with NULL
   *
   * Example:
   * @code
   * const char *third_param  = "third";
   * const char *const args[] = { "hello", "world", third_param, NULL };
   * set_argv_strings (args);
   * @endcode
   */
  void
  set_argv_strings (const char *const argv[])
  {
    this->_argv = argv;
  }

  /**
   * Preparing the POSIX environment that the process will receive.
   * WARNING: Do not confuse this environment with the L4Re::env !
   *
   * The environment must be pushed on the stack of the program,
   * because the loader expects it to be in reverse order and
   * it should make up a continuos region of memory.
   *
   * argv.al should point to the last string and
   * argv.a0 should point to the first string
   *
   * a0 == 0 means there are no arguments
   */
  void
  push_env_strings ()
  {
    // make sure the argument list exists and is not empty
    if (_envp == NULL || *_envp == NULL)
      return;

    // push_str returns the start address of the string on the stack
    envp.al = _stack.push_str (*_envp, strlen (*_envp));
    envp.a0 = envp.al;

    // loop through args and push them on the stack
    for (const char *const *env = _envp + 1; *env != NULL; env++)
      envp.al = _stack.push_str (*env, strlen (*env));
  }

  /**
   * Set the POSIX environment values that the program will receive.
   * WARNING: Do not confuse this environment with the L4Re::env !
   *
   * @param _envp This array of strings needs to be terminated with NULL
   *
   * Example:
   * @code
   * const char *third_param  = "third";
   * const char *const envp[] = { "hello", "world", third_param, NULL };
   * set_envp_strings (envp);
   * @endcode
   */
  void
  set_envp_strings (const char *const envp[])
  {
    this->_envp = envp;
  }

  /**
   * This method expects a NULL terminated array of Cap objects.
   * These objects are used to configure which Capabilities are transferred
   * to the new Process.
   */
  void
  set_capabilities (const Cap *const capabilities[])
  {
    this->_capabilities = capabilities;
  }
};

int execve (const char *file, const char *const argv[] = NULL,
            const char *const envp[] = NULL,
            const Cap *const capabilities[] = NULL);