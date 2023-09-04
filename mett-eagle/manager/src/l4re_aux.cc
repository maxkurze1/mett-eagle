/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
/**
 * @file
 * This file is necessary for finding the l4re_aux pointer which will be used
 * by the loader
 * 
 * The code is mostly copied from l4re-core/l4re_kernel/server/src/globals.cc
 */

#include <l4/crtn/initpriorities.h>
#include <l4/re/l4aux.h>

// internal uclibc symbol for ENV
extern char const **__environ;

/**
 * Absolutely no clue what this does..
 * Something with the kip ...
 * KIP === Kernel Interface Page
 *
 * Used by the loader somehow
 */
l4re_aux_t *l4re_aux;

static void
init ()
{
  char const *const *envp = __environ;
  l4_umword_t const *auxp = reinterpret_cast<l4_umword_t const *> (envp);
  while (*auxp)
    ++auxp;
  ++auxp;

  while (*auxp)
    {
      if (*auxp == 0xf0)
        l4re_aux = (l4re_aux_t *)auxp[1];
      auxp += 2;
    }
}

/**
 * This will execute the init function before the main will be invoked
 */
L4_DECLARE_CONSTRUCTOR (init, INIT_PRIO_EARLY)