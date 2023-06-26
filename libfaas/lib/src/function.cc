#include <l4/re/error_helper>
#include <l4/liblog/log>
#include <string.h>
#include <l4/re/env>
#include <l4/libfaas/faas>
#include <l4/mett-eagle/util>
#include <l4/mett-eagle/alias>

// global objects
static L4Re::MettEagle::Single_server<> server;
static L4Re::Util::Object_registry reg {&server};
static L4::Cap<MettEagle::Manager_Worker> manager {L4::cap_cast<MettEagle::Manager_Worker> (L4Re::Env::env ()->parent ())};

/**
 * @brief Wrapper main that will handle the MettEagle manager interaction
 */
int
main (int argc, const char *argv[])
try
  {
    /* there has to be exactly one argument -- the string passed */
    if (L4_UNLIKELY (argc != 1))
      throw L4::Runtime_error (-L4_EINVAL, "Wrong arguments");
    
    /* actual call to the faas function */
    std::string ret {Main(argv[0])};

    /* the default _exit implementation can only return an integer *
     * to pass a string the custom manager->exit must be used.     */
    L4Re::chksys(manager->exit(ret.c_str()));

    throw L4::Runtime_error (-L4_EFAULT, "Wrapper main should never get here!");
  }
catch (L4::Runtime_error &e)
  {
    /**
     * This catch block will catch errors that are thrown by utility
     * methods like L4Re::chkcap or by own methods
     */
    log_fatal (e);
    return e.err_no(); // propagate the error to the client
  }

std::string L4Re::Faas::invoke(std::string name) {
  manager->action_invoke(name.c_str());
}