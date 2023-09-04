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
#include <l4/mett-eagle/util>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/unique_cap>

#include <l4/re/error_helper>

#include <l4/fmt/core.h>
#include <l4/fmt/ranges.h>

#include <cstdio>
#include <string>

#include <time.h>

#include <algorithm>
#include <numeric>

namespace MettEagle = L4Re::MettEagle;
using namespace L4Re::LibLog;

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
  avg () { return D_TYPE (sum ()) / D_TYPE (samples.size ()); }

  std::size_t
  cnt ()
  {
    return samples.size ();
  }

  std::string
  toString ()
  {
    return fmt::format ("{{\"sum\":{},\"min\":{},\"max\":{},\"avg\":{},"
                        "\"cnt\":{},\n\"samples\":{}}}",
                        sum (), min (), max (), avg (), cnt (), samples);
    // return fmt::format("{}", samples);
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

static void
benchmark (const char *action_name, int iterations, Metrics *metrics)
try
  {
    /* setup */
    auto manager = MettEagle::getManager ("manager");
    L4Re::chksys (manager->action_create ("testAction", action_name),
                  "action create");

    for (int i = 0; i < iterations; i++)
      {
        /* invocation */
        auto before_invocation = std::chrono::high_resolution_clock::now ();

        std::string answer;
        MettEagle::Metadata data;
        L4Re::chksys (manager->action_invoke ("testAction", "some argument",
                                              answer, {}, &data),
                      "action invoke");

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
    log<FATAL> ("{}", e);
  }
catch (L4::Runtime_error &e)
  {
    log<FATAL> ("{}", e);
  }

static constexpr int THREAD_NUM = 12;
static constexpr int ITERATIONS = 100;

Metrics metrics_arr[THREAD_NUM];

std::list<std::thread> threads;

int
main (const int /* argc */, const char *const /* argv */[])
try
  {
    /* increase log semaphore once to prevent deadlock ... TODO this would fit
     * better into the .cfg */
    L4Re::Env::env ()->get_cap<L4::Semaphore> ("log_sync")->up ();

    log<INFO> ("Hello from client");

    /* start threads */
    for (int i = 0; i < THREAD_NUM; i++)
      threads.push_back (std::thread (benchmark, "rom/function1", ITERATIONS,
                                      &metrics_arr[i]));

    /* join threads */
    for (std::thread &t : threads)
      t.join ();

    printf ("====   OUTPUT   ====\n");
    for (int i = 0; i < THREAD_NUM; i++)
        printf ("%s\n", metrics_arr[i].toString ().c_str ());
    printf ("==== END OUTPUT ====\n");

    return EXIT_SUCCESS;
  }
catch (L4Re::LibLog::Loggable_exception &e)
  {
    log<FATAL> ("{}", e);
    return e.err_no ();
  }
catch (L4::Runtime_error &e)
  {
    log<FATAL> ("{}", e);
    return e.err_no ();
  }