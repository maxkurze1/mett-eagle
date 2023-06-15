#include <l4/re/error_helper>
#include <l4/liblog/log>
#include <l4/mett-eagle/manager>
#include <l4/re/env>
#include <string.h>
#include <l4/libfaas/util>


/**
 * @brief Wrapper main that will handle the MettEagle manager interaction
 */
int
main (int argc, const char *argv[])
try
  {
    /* there has to be exactly one argument -- the json string passed */
    if (L4_UNLIKELY (argc != 1))
      throw L4::Runtime_error (-L4_EINVAL, "Wrong arguments");
    
    /* actual call to the faas function */
    std::string ret {Main(argv[0])};

    /* the default _exit implementation can only return an integer *
     * to pass a string the custom manager->exit must be used.     */
    L4::Cap<MettEagle::Manager_Worker> manager
        = L4::cap_cast<MettEagle::Manager_Worker> (L4Re::Env::env ()->parent ());
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
    return 1; // maybe propagate the error to the client?
  }