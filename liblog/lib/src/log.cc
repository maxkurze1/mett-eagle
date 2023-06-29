/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <cstdlib>
#include <ctime>
#include <l4/liblog/log>

L4Re::LibLog::Level L4Re::LibLog::_level;

int
L4Re::LibLog::put_severity (MsgLevel msg_lvl, FILE *output)
{
  switch (msg_lvl)
    {
    case MsgLevel::DEBUG:
      return fputs ("\033[34;1mDEBUG\033[0m", output);
    case MsgLevel::INFO:
      return fputs ("\033[36;1mINFO \033[0m", output);
    case MsgLevel::WARN:
      return fputs ("\033[33;1mWARN \033[0m", output);
    case MsgLevel::ERROR:
      return fputs ("\033[31;1mERROR\033[0m", output);
    case MsgLevel::FATAL:
      return fputs ("\033[35;1mFATAL\033[0m", output);
    }
  return 0;
}

int
L4Re::LibLog::put_time (FILE *output)
{

  struct timespec ts;
  if (clock_gettime (CLOCK_REALTIME, &ts))
    return 0;

  // int minutes = ts.tv_sec / 60;
  // int seconds = ts.tv_sec % 60;
  // print sec : usec per default
  return fprintf (output, "%02ld:%03ld", ts.tv_sec, ts.tv_nsec / 1000000);
}

int
L4Re::LibLog::put_pkgname (FILE *output)
{
  const char *const name = getenv ("PKGNAME");
  if (name == NULL)
    return 0;
  return fputs (name, output);
}