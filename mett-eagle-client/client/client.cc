/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/liblog/log>
#include <l4/mett-eagle/alias>
#include <l4/mett-eagle/manager>
#include <l4/mett-eagle/util>

#include <l4/re/error_helper>

using L4Re::LibLog::Log;

int
main (const int _argc, const char *const _argv[])
try
  {
    (void)_argc;
    (void)_argv;

    Log::debug ("Hello from client c version %ld", __cplusplus);

    auto manager = MettEagle::getManager ("manager");

    Log::debug ("Register done");

    L4Re::chksys(manager->action_create ("test", "rom/function1"), "action create");
    L4Re::chksys(manager->action_create ("test_idk", "rom/function2"), "action create");
    
    Log::debug ("create done");
    
    std::string answer;
    L4Re::chksys (manager->action_invoke ("test", answer), "action invoke");

    Log::debug("got answer %s", answer.c_str());

    return EXIT_SUCCESS;
  }
catch (L4Re::LibLog::Loggable_base_exception &e)
  {
    Log::fatal {e};
    return e.err_no ();
  }
catch (L4::Runtime_error &e)
  {
    Log::fatal {e};
    return e.err_no ();
  }