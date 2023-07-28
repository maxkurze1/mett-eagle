/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
#include <list>
#include <memory>
#include <thread>

#include <l4/liblog/error_helper>
#include <l4/liblog/log>
#include <l4/mett-eagle/manager>
#include <l4/mett-eagle/util>

#include <l4/re/error_helper>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <atomic>

namespace MettEagle = L4Re::MettEagle;
using L4Re::LibLog::Log;

std::atomic<int> x {0};

static void
client (const char *action_name, int num)
try
  {
    // sync clients
    x ++;
    while (x != 2) {std::this_thread::yield();};



    // auto file
    //     = L4Re::Util::Env_ns{}.query<L4Re::Dataspace> ("rwfs/output.txt");
    // if (!file.is_valid ())
    //   throw L4Re::LibLog::Loggable_exception (-L4_EINVAL, "Failed to query");
    // // TODO add chkcap everywhere
    // auto data = L4Re::chkcap (L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> (), "no cap");
    // L4Re::chksys(L4Re::Env::env()->mem_alloc()->alloc(4096,data), "malloc");
    // l4_addr_t _addr;
    // L4Re::chksys(L4Re::Env::env()->rm()->attach(&_addr, data->size(), L4Re::Rm::F::RW | L4Re::Rm::F::Search_addr, L4::Ipc::make_cap_rw(data)), "attach");
    // const char *msg = "Some text";
    // memcpy((void *)_addr, msg, strlen(msg) + 1);
    // L4Re::chksys(file->copy_in (0, data, 0, strlen(msg) + 1), "copy in");

    // l4_addr_t _addr_file;
    // L4Re::chksys(L4Re::Env::env()->rm()->attach(&_addr_file, file->size(), L4Re::Rm::F::RW | L4Re::Rm::F::Search_addr, L4::Ipc::make_cap_rw(file)), "file attach");
    
    // Log::debug(fmt::format("file content = {:s}", (char *)_addr_file));


    // FILE *fptr;
    // // Open a file in writing mode
    // fptr = fopen ("rwfs/output.txt", "r+");
    // if (fptr == NULL)
    //   throw L4Re::LibLog::Loggable_exception (-L4_EINVAL, "Failed to open");
    // // Write some text to the file
    // if (fprintf (fptr, "Some text") < 0)
    //   throw L4Re::LibLog::Loggable_exception (-L4_EINVAL, "Failed to write");
    // // char buffer[100];
    // // fread(buffer, 100, 1, fptr);
    // // Close the file
    // fclose (fptr);


    // Log::debug (fmt::format ("Read from file {:s}", buffer));

    // std::fstream output_file ("rwfs/output.txt");
    // if (!output_file)
    //   throw L4Re::LibLog::Loggable_exception (-L4_EINVAL,
    //                                           "Failed to open file");
    // output_file << "Writing this to a file.\n";
    // output_file.close ();

    auto manager = MettEagle::getManager ("manager");


    // sync clients
    // x ++;
    // while (x != 4) {std::this_thread::yield();};


    Log::debug ("Register done");

    L4Re::chksys (manager->action_create ("name", action_name),
                  "action create");

    // Log::debug ("create done");

    // std::string answer;
    // L4Re::chksys (manager->action_invoke ("name", answer), "action invoke");

    // Log::debug (fmt::format ("got answer {:s}", answer.c_str ()));
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    Log::fatal (fmt::format ("{}", e));
  }
catch (L4::Runtime_error &e)
  {
    Log::fatal (fmt::format ("{}", e));
  }

int
main (const int _argc, const char *const _argv[])
try
  {
    (void)_argc;
    (void)_argv;
    L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync")->up ();

    std::list<std::unique_ptr<std::thread> > thread_list;

    // std::thread t1(client, "rom/function1");

    for (int i = 0; i < 2; i++)
      {
        thread_list.push_back (std::make_unique<std::thread> (
            client, i == 0 ? "rom/function1" : "rom/function1", i));
      }

    for (auto &t : thread_list)
      {
        t->join ();
      }

    return EXIT_SUCCESS;
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    Log::fatal (fmt::format ("{}", e));
    return e.err_no ();
  }
catch (L4::Runtime_error &e)
  {
    Log::fatal (fmt::format ("{}", e));
    return e.err_no ();
  }