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

#include <l4/fmt/core.h>
#include <l4/fmt/ranges.h>

#include <cstdio>
#include <cstdlib>
#include <pthread-l4.h>
#include <pthread.h>
#include <string>
#include <time.h>

namespace MettEagle = L4Re::MettEagle;
using namespace L4Re::LibLog;

static int THREAD_NUM = 1;   // 63
static int ITERATIONS = 200; // 901 is fine
static int waiting_time_ms = 10000;

/**
 * @brief A class that represents a single metric
 *
 * An example metric could be 'execution time'
 *
 * @tparam TYPE    The type of the measurements
 * @tparam D_TYPE  The type used for division
 */
template <typename TYPE = l4_uint64_t, typename D_TYPE = double> struct Metric
{
  std::list<TYPE> samples;

  void
  addSample (TYPE val)
  {
    samples.push_back (val);
  }

  TYPE
  sum ()
  {
    return std::accumulate (samples.begin (), samples.end (), TYPE (0));
  }

  TYPE
  min ()
  {
    return *std::min_element (samples.begin (), samples.end ());
  }

  TYPE
  max ()
  {
    return *std::max_element (samples.begin (), samples.end ());
  }

  D_TYPE
  median ()
  {
    if (samples.size () == 0)
      return D_TYPE (0);
    samples.sort ();
    if (samples.size () % 2 == 0)
      { // no exact median
        auto middle = samples.size () / 2;
        return (samples[middle] + samples[middle - 1]) / D_TYPE (2);
      }
    else
      return samples[(samples.size () - 1) / 2];
  }

  D_TYPE
  avg ()
  {
    if (samples.size () == 0)
      return D_TYPE (0);
    return D_TYPE (sum ()) / D_TYPE (samples.size ());
  }

  std::size_t
  cnt ()
  {
    return samples.size ();
  }

  std::string
  toString ()
  {
    // return fmt::format ("{{\"sum\":{},\"min\":{},\"max\":{},\"avg\":{},"
    //                     "\"cnt\":{},\n\"samples\":{}}}",
    //                     sum (), min (), max (), avg (), cnt (), samples);
    return fmt::format ("{}", samples);
  }
};

/**
 * These metrics try to resemble the ones from owperf
 * https://github.com/IBM/owperf
 */
struct Metrics
{
  Metric<> d;
  Metric<> bi;
  Metric<> ai;
  Metric<> as;
  Metric<> ae;
  Metric<> ad;
  Metric<> oea;
  Metric<> oer;
  Metric<> ora;
  Metric<> rtt;
  Metric<> ortt;

  std::string
  toString ()
  {
    return fmt::format ("{{\n"
                        "\"d\" : {},\n"
                        "\"bi\" : {},\n"
                        "\"ai\" : {},\n"
                        "\"as\" : {},\n"
                        "\"ae\" : {},\n"
                        "\"ad\" : {},\n"
                        "\"oea\" : {},\n"
                        "\"oer\" : {},\n"
                        "\"ora\" : {},\n"
                        "\"rtt\" : {},\n"
                        "\"ortt\" : {}\n"
                        "}}",
                        d.toString (), bi.toString (), ai.toString (),
                        as.toString (), ae.toString (), ad.toString (),
                        oea.toString (), oer.toString (), ora.toString (),
                        rtt.toString (), ortt.toString ());
  }
};

std::mutex mutex;
/* this variable will be used to make sure all client start at the same time */
std::condition_variable sync_clients;
std::atomic<int> waiting_count = 0;

/* this variable ensures that the clients are registered in serial */
std::condition_variable serialize_clients;

static void
benchmark (const char *action_name, Metrics *metrics)
try
  {
    /* setup */
    auto manager = MettEagle::getManager ("manager");
    L4Re::chksys (manager->action_create ("testAction", action_name),
                  "action create");

    std::unique_lock<std::mutex> lk (mutex);
    waiting_count++;
    serialize_clients.notify_one ();
    sync_clients.wait (lk, [] { return waiting_count == THREAD_NUM; });
    lk.unlock ();
    sync_clients.notify_one ();

    for (int i = 0; i < ITERATIONS; i++)
      {
        log<DEBUG> ("Iteration {}", i);

        /* invocation */
        auto before_invocation = std::chrono::high_resolution_clock::now ();

        std::string answer;
        MettEagle::Metadata data;
        try
          {
            L4Re::chksys (manager->action_invoke (
                              "testAction",
                              std::to_string (waiting_time_ms).c_str (),
                              answer, {}, &data),
                          "action invoke");
          }
        catch (...)
          {
            log<ERROR> ("action invoke with error");
            i--;
            continue;
          }

        auto after_invocation = std::chrono::high_resolution_clock::now ();

        /* duration measured inside the application */
        unsigned long d = std::stoul (answer);

        /**
         * The metrics will be calculated with a granularity of microseconds
         * because thats the precision of the kernel clock
         */
        // clang-format off
        unsigned long bi = std::chrono::duration_cast<std::chrono::microseconds> (
                                before_invocation.time_since_epoch ()).count ();
        unsigned long ai = std::chrono::duration_cast<std::chrono::microseconds> (
                                after_invocation.time_since_epoch ()).count ();
        unsigned long as = std::chrono::duration_cast<std::chrono::microseconds> (
                                data.start.time_since_epoch ()).count ();
        unsigned long ae = std::chrono::duration_cast<std::chrono::microseconds> (
                                data.end.time_since_epoch ()).count ();
        // clang-format on

        unsigned long ad = ae - as;
        unsigned long oea = as - bi;
        unsigned long oer = ae - bi - d;
        unsigned long ora = ai - ae;
        unsigned long rtt = ai - bi;
        unsigned long ortt = rtt - d;

        // update metrics

        metrics->d.addSample (d);
        metrics->bi.addSample (bi);
        metrics->ai.addSample (ai);
        metrics->as.addSample (as);
        metrics->ae.addSample (ae);
        metrics->ad.addSample (ad);
        metrics->oea.addSample (oea);
        metrics->oer.addSample (oer);
        metrics->ora.addSample (ora);
        metrics->rtt.addSample (rtt);
        metrics->ortt.addSample (ortt);
      }
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    log<FATAL> (e);
  }
catch (L4::Runtime_error &e)
  {
    log<FATAL> (e);
  }

/* struct to pass arguments to pthread function*/
struct pthread_args
{
  const char *action_name;
  Metrics *metrics;
};
/* pthread wrapper for calling the benchmark function */
static void *
pthread_benchmark (void *_arg)
{
  auto args = (pthread_args *)_arg;
  benchmark (args->action_name, args->metrics);
  return NULL;
}

Metrics *metrics_arr;

std::list<std::thread> threads;

#include <l4/sys/debugger.h>
#include <thread-l4>

int
main (const int argc, const char *const argv[])
try
  {
    L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync")->up ();

    auto manager = MettEagle::getManager ("manager");

    log<INFO> ("Client hello");

    L4Re::chksys (manager->action_create ("function", "rom/function.py",
                                          MettEagle::Language::PYTHON));
    L4Re::chksys (manager->action_create ("function2", "rom/function2.py",
                                          MettEagle::Language::PYTHON));

    log<INFO> ("actions created");

    std::string ret;
    L4Re::chksys (manager->action_invoke ("function", "some param", ret));

    log<INFO> ("action invoked, ret = {}", ret);

    L4Re::chksys (manager->action_invoke ("function", "some param", ret));

    log<INFO> ("action invoked twice, ret = {}", ret);
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

static int
main_benchmark (const int argc, const char *const argv[])
try
  {
    l4_debugger_set_object_name (L4Re::Env::env ()->task ().cap (),
                                 "clnt"); // mett-eagle client
    l4_debugger_set_object_name (L4Re::Env::env ()->main_thread ().cap (),
                                 "clnt main");

    /* increase log semaphore once to prevent deadlock ... TODO this would fit
     * better into the .cfg */
    L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync")->up ();

    log<INFO> ("{:d} passed args:", argc);
    for (int i = 0; i < argc; i++)
      {
        log<INFO> ("   {:d}: {:s}", i, argv[i]);
      }

    /* 0th argument == name of the binary => ignored */

    /* first argument == number of client threads */
    if (argc >= 2)
      {
        THREAD_NUM = std::stol (argv[1]);
      }
    log<INFO> ("Thread number set to {}", THREAD_NUM);

    /* second argument == number of iterations */
    if (argc >= 3)
      {
        ITERATIONS = std::stol (argv[2]);
      }
    log<INFO> ("Iteration count set to {}", ITERATIONS);

    // TODO pass everything else to the function ??

    metrics_arr = new Metrics[THREAD_NUM];
    if (metrics_arr == NULL)
      throw Loggable_exception (-L4_ENOMEM, "malloc");

    /* start threads c++ threads */
    // for (int i = 0; i < THREAD_NUM; i++)
    //   {
    //     std::thread thread (benchmark, "rom/function1", &metrics_arr[i]);
    //     l4_debugger_set_object_name (std::L4::thread_cap (thread).cap (),
    //                                  fmt::format ("me clnt {}", i).c_str
    //                                  ());
    //     threads.push_back (std::move (thread));
    //   }

    /* start threads pthreads */
    pthread_t pthread[THREAD_NUM];
    pthread_args args[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; i++)
      {
        std::unique_lock<std::mutex> lk (mutex);
        serialize_clients.wait (lk, [=] { return waiting_count == i; });
        lk.unlock ();

        pthread_attr_t attr;
        pthread_attr_init (&attr);

        attr.create_flags |= PTHREAD_L4_ATTR_NO_START;

        args[i].action_name = "rom/function1";
        args[i].metrics = &metrics_arr[i];

        int failed
            = pthread_create (&pthread[i], &attr, pthread_benchmark, &args[i]);
        if (failed)
          throw Loggable_exception (-errno, "failed to create thread");

        pthread_attr_destroy (&attr);

        auto sched_cap = L4Re::Util::make_shared_cap<L4::Scheduler> ();

        l4_umword_t bitmap = 0b10LL << i;
        // l4_umword_t bitmap = 0b1;

        chksys (l4_msgtag_t (
                    L4Re::Env::env ()->user_factory ()->create<L4::Scheduler> (
                        sched_cap.get ())
                    << l4_mword_t (L4_SCHED_MAX_PRIO)
                    << l4_mword_t (L4_SCHED_MIN_PRIO) << bitmap),
                "Failed to create scheduler");

        auto thread_cap = L4::Cap<L4::Thread> (pthread_l4_cap (pthread[i]));

        log<DEBUG> ("running client on cpu {:#b}", bitmap);

        chksys (sched_cap->run_thread (
            thread_cap, l4_sched_param (L4RE_MAIN_THREAD_PRIO)));
      }

    /* join threads c++*/
    // for (std::thread &t : threads)
    //   t.join ();

    /* join pthreads */
    for (int i = 0; i < THREAD_NUM; i++)
      pthread_join (pthread[i], NULL);

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