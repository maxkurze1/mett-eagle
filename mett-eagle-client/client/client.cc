/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
#include <algorithm>
#include <condition_variable>
#include <list>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include <l4/liblog/error_helper>
#include <l4/liblog/log>
#include <l4/mett-eagle/util>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/shared_cap>
#include <l4/re/util/unique_cap>
#include <l4/sys/scheduler>
#include <l4/sys/thread>

#include <l4/fmt/chrono.h>
#include <l4/fmt/core.h>
#include <l4/fmt/ranges.h>

#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <pthread-l4.h>
#include <pthread.h>
#include <string>
#include <time.h>

namespace MettEagle = L4Re::MettEagle;
using namespace L4Re::LibLog;

static int THREAD_NUM = 64;
static int ITERATIONS = 1000;
// static int waiting_time_ms = 50;

/**
 * These metrics that are measured by each thread (client) separately
 */
struct Metrics
{
  std::list<std::chrono::microseconds> start_invocation;
  std::list<std::chrono::microseconds> end_invocation;
  std::list<std::chrono::microseconds> start_worker;
  std::list<std::chrono::microseconds> end_worker;
  std::list<std::chrono::microseconds> start_runtime;
  std::list<std::chrono::microseconds> end_runtime;
  std::list<std::chrono::microseconds> start_function;
  std::list<std::chrono::microseconds> end_function;

  std::list<std::chrono::microseconds> function_internal_duration;

  std::string
  toString ()
  {
    return fmt::format ("{{\n"
                        "\"invocation\": {{\n"
                        "  \"start\": {::%Q},\n"
                        "  \"end\"  : {::%Q}\n"
                        "}},\n"
                        "\"worker\": {{\n"
                        "  \"start\": {::%Q},\n"
                        "  \"end\"  : {::%Q}\n"
                        "}},\n"
                        "\"runtime\": {{\n"
                        "  \"start\": {::%Q},\n"
                        "  \"end\"  : {::%Q}\n"
                        "}},\n"
                        "\"function\": {{\n"
                        "  \"start\": {::%Q},\n"
                        "  \"end\"  : {::%Q},\n"
                        "  \"internal_duration\": {::%Q}\n"
                        "}}\n"
                        "}}",
                        start_invocation, end_invocation, start_worker,
                        end_worker, start_runtime, end_runtime, start_function,
                        end_function, function_internal_duration);
  }
};

std::mutex mutex;
/* this variable will be used to make sure all client start at the same time */
std::condition_variable sync_clients;
std::atomic<int> waiting_count = 0;

/* this variable ensures that the clients are registered in serial */
std::condition_variable serialize_clients;

static void
benchmark (const char *action_name, Metrics *metrics, int id)
try
  {
    auto manager = MettEagle::getManager ("manager");

    L4Re::chksys (manager->action_create ("testAction", action_name,
                                          MettEagle::Language::BINARY),
                  "action create");

    log<INFO> ("registered client {}", waiting_count.load ());

    /* notify main thread to start next client */
    std::unique_lock<std::mutex> lk (mutex);
    waiting_count++;
    serialize_clients
        .notify_one (); // let the next client register and create an action

    // wait until all clients are registered and created an action
    sync_clients.wait (lk, [] { return waiting_count == THREAD_NUM; });
    lk.unlock ();
    sync_clients.notify_one ();

    /* this is the actual measurement loop */
    for (int i = 0; i < ITERATIONS; i++)
      {
        /* invocation */
        auto start_invocation = std::chrono::high_resolution_clock::now ();

        std::string answer;
        MettEagle::Metadata data;
        try
          {
            L4Re::chksys (manager->action_invoke (
                              "testAction",
                              "", // std::to_string (waiting_time_ms).c_str (),
                              answer, MettEagle::Config{.timeout_us = 75'000}, &data),
                          "action invoke");
          }
        catch (...)
          {
              // log<ERROR> ("action invoke with error (i = {:d}),
              // retrying...", i);
              i--;
              continue;
          }

        auto end_invocation = std::chrono::high_resolution_clock::now ();

        /**
         * The metrics will be calculated with a granularity of microseconds
         * because thats the precision of the kernel clock
         */
        // clang-format off
        metrics->start_invocation.push_back(std::chrono::duration_cast<std::chrono::microseconds>(     start_invocation.time_since_epoch()));
        metrics->end_invocation  .push_back(std::chrono::duration_cast<std::chrono::microseconds>(     end_invocation  .time_since_epoch()));
        metrics->start_worker    .push_back(std::chrono::duration_cast<std::chrono::microseconds>(data.start_worker    .time_since_epoch()));
        metrics->end_worker      .push_back(std::chrono::duration_cast<std::chrono::microseconds>(data.end_worker      .time_since_epoch()));
        metrics->start_runtime   .push_back(std::chrono::duration_cast<std::chrono::microseconds>(data.start_runtime   .time_since_epoch()));
        metrics->end_runtime     .push_back(std::chrono::duration_cast<std::chrono::microseconds>(data.end_runtime     .time_since_epoch()));
        metrics->start_function  .push_back(std::chrono::duration_cast<std::chrono::microseconds>(data.start_function  .time_since_epoch()));
        metrics->end_function    .push_back(std::chrono::duration_cast<std::chrono::microseconds>(data.end_function    .time_since_epoch()));
        // clang-format on

        /* duration measured inside the application */
        // metrics->function_internal_duration.push_back (
        //     std::chrono::microseconds (std::stoul (answer)));

        metrics->function_internal_duration.push_back (
            std::chrono::microseconds (0));
          }
      }
    catch (L4Re::LibLog::Loggable_exception &e) { log<FATAL> (e); }
    catch (L4::Runtime_error &e) { log<FATAL> (e); }

    /* struct to pass arguments to pthread function*/
    struct pthread_args
    {
      const char *action_name;
      Metrics *metrics;
      int id;
    };

    /* pthread wrapper for calling the benchmark function */
    static void *pthread_benchmark (void *_arg)
    {
      auto args = (pthread_args *)_arg;
      benchmark (args->action_name, args->metrics, args->id);
      return NULL;
    }

    Metrics *metrics_arr;

    std::list<std::thread> threads;

#include <l4/sys/debugger.h>
#include <thread-l4>

    int main (const int argc, char *const argv[])
    try
      {
        /* increase log semaphore once to prevent deadlock ... TODO this would
         * fit better into the .cfg, but currently there seems to be no
         * semaphore:up() for lua*/
        L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync")->up ();

        // l4_debugger_set_object_name (L4Re::Env::env ()->task ().cap (),
        //  "clnt"); // mett-eagle client
        // l4_debugger_set_object_name (L4Re::Env::env ()->main_thread ().cap
        // (), "clnt main");

        /* parsing command line options using GNUs getopt */

        // clang-format off
        option long_options[] = {
          { "iterations", required_argument, nullptr, 'i' },
          { "threads",    required_argument, nullptr, 't' },
          { "help",       no_argument,       nullptr, 'h' },
          { nullptr,      0,                 nullptr, 0   },
        };
        // clang-format on

        opterr = 0; // do not print default error message
        for (int option, index;
             (option = getopt_long (argc, argv, "i:t:h", long_options, &index))
             != -1;)
          switch (option)
            {
            case 't':
              THREAD_NUM = std::stol (optarg);
              break;
            case 'i':
              ITERATIONS = std::stol (optarg);
              break;
            default:
              log<INFO> ("unknown option '{:s}'", argv[optind - 1]);
              [[fallthrough]];
            case 'h':
              log<INFO> ("{:s} - a benchmark client for the mett-eagle server",
                         argv[0]);
              log<INFO> ("USAGE:");
              log<INFO> ("{:s} [OPTION]...", argv[0]);
              log<INFO> ("OPTIONS");
              log<INFO> ("  -i --iterations=NUM");
              log<INFO> ("    each thread should do NUM invocations");
              log<INFO> ("  -t --threads=NUM");
              log<INFO> ("    NUM threads should be spawned");
              log<INFO> ("  -h --help");
              log<INFO> ("    show this help message");
              exit (0);
            }

        log<INFO> ("Thread number set to {}", THREAD_NUM);
        log<INFO> ("Iteration count set to {}", ITERATIONS);

        // TODO pass remaining arguments to the function or not??

        metrics_arr = new Metrics[THREAD_NUM];
        if (metrics_arr == NULL)
          throw Loggable_exception (-L4_ENOMEM, "malloc");

        /* start threads pthreads */
        pthread_t pthread[THREAD_NUM];
        pthread_args args[THREAD_NUM];
        for (int i = 0; i < THREAD_NUM; i++)
          {
            /* wait until the client of the previous iteration has registered
             * his action */
            /* this serialization is only necessary to make sure that the
             * server will assign the available cpus to the clients in a
             * predictable manner */
            std::unique_lock<std::mutex> lk (mutex);
            serialize_clients.wait (lk, [=] { return waiting_count == i; });
            lk.unlock ();

            pthread_attr_t attr;
            pthread_attr_init (&attr);

            attr.create_flags |= PTHREAD_L4_ATTR_NO_START;

            args[i].action_name = "rom/function1";
            args[i].metrics = &metrics_arr[i];
            args[i].id = i;

            int failed = pthread_create (&pthread[i], &attr, pthread_benchmark,
                                         &args[i]);
            if (failed)
              throw Loggable_exception (-errno, "failed to create thread");

            pthread_attr_destroy (&attr);

            auto sched_cap = L4Re::Util::make_shared_cap<L4::Scheduler> ();

            // l4_umword_t bitmap = 0b10LL << i;
            l4_umword_t bitmap = 0b1;

            chksys (
                l4_msgtag_t (
                    L4Re::Env::env ()->user_factory ()->create<L4::Scheduler> (
                        sched_cap.get ())
                    << l4_mword_t (L4_SCHED_MAX_PRIO)
                    << l4_mword_t (L4_SCHED_MIN_PRIO) << bitmap),
                "Failed to create scheduler");

            auto thread_cap
                = L4::Cap<L4::Thread> (pthread_l4_cap (pthread[i]));

            log<DEBUG> ("running client on cpu {:#b}", bitmap);

            chksys (sched_cap->run_thread (
                thread_cap, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));
          }

        /* join pthreads */
        for (int i = 0; i < THREAD_NUM; i++)
          {
            pthread_join (pthread[i], NULL);
          }

        /* generate output */
        printf ("====   OUTPUT   ====\n[");
        for (int i = 0; i < THREAD_NUM; i++)
          printf ("%s\n%c", metrics_arr[i].toString ().c_str (),
                  i == THREAD_NUM - 1 ? ' ' : ',');
        printf ("]\n==== END OUTPUT ====\n");

        delete[] metrics_arr;

        return EXIT_SUCCESS;
      }
    catch (L4Re::LibLog::Loggable_exception &e)
      {
        log<FATAL> (e);
        return e.err_no ();
      }
    catch (L4::Runtime_error &e)
      {
        log<FATAL> (e);
        return e.err_no ();
      }