#include <l4/cxx/iostream>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/debug>
#include <l4/util/util.h>
#include <stdlib.h>
#include <string.h>

L4Re::Util::Dbg dbg (0x1, "Mett-Eagle", "Worker");

int
main (int argc, const char *argv[])
try
  {
    dbg.printf ("Hello from worker! args:\n");
    for (int i = 0; i < argc; i++)
      {
        dbg.printf ("%s\n", argv[i]);
      }

    // try accessing the posix environment variables
    dbg.printf ("ENV 'test': %s\n", getenv ("test"));
    const char *r = "cap_name";
    L4::Cap<L4Re::Log> log
        = L4Re::Env::env ()->get_cap<L4Re::Log> (r, strlen (r));
    dbg.printf ("Capa %d\n", log.is_valid ());
    // log->print ("Helloooo with fucking capability\n");
    throw L4::Runtime_error (1, "some err");
    // r = "other_name";
    log = L4Re::chkcap (
        /* L4Re::Env::env ()->get_cap<L4Re::Log> (r, strlen (r)) */ log,
        "failed to get capability\n");
    // log->print ("Helloooo with fucking capability\n");

    return 0;
  }
catch (/* L4::Runtime_error &e */...)
  {
    /**
     * This catch block will catch error that are thrown by utility
     * methods like L4Re::chkcap
     */
    // L4::cerr << "FATAL: " << e;
    L4::cerr << "Fatal: error";
    return 1;
  }