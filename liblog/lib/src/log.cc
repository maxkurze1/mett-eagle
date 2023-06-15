#include <cstdlib>
#include <l4/liblog/log>
#include <ctime>

Log::Level Log::_level;

int
Log::put_severity (MsgLevel msg_lvl, FILE *output)
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
Log::put_time (FILE *output)
{
  // TODO this somehow doesn't work
  time_t now = time (0);
  tm *tm = localtime (&now);
  char buffer[6]; // always adjust buffer size when changing output format!!
  strftime (buffer, sizeof (buffer), "%H:%M", tm);
  return fprintf (output, "%s", buffer);
}

int
Log::put_pkgname (FILE *output)
{
  const char *const name = getenv ("PKGNAME");
  if (name)
    {
      return fputs(name, output);
    }
  return -L4_ENOENT;
}