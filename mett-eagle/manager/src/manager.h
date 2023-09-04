// -*- Mode: C++ -*-
// vim:ft=cpp
/**
 * (c) 2023 Max Kurze <max.kurze@mailbox.tu-dresden.de>
 *
 * This file is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the LICENSE.md file for details.
 */
/**
 * @file
 * Globals provided by the manager.cc for internal usage
 * and common declarations
 */

#pragma once

#include <l4/liblog/log>
#include <l4/liblog/loggable-exception>

#include <l4/re/error_helper>

// clang-format off
/* necessary for the namespace alias */
namespace L4Re { namespace MettEagle {} }
namespace MettEagle = L4Re::MettEagle;
// clang-format on

// TODO use own  -- using L4Re::LibLog::chksys;
using L4Re::chkcap;
using L4Re::chkipc;
using L4Re::chksys;

using L4Re::LibLog::Loggable_exception;
using namespace L4Re::LibLog;

#include <bitset>
#include <l4/sys/scheduler>
/**
 * Cpu bitmap of cpus accessible to the manager process
 * that are not currently assigned to clients.
 */
extern std::bitset<sizeof (l4_sched_cpu_set_t::map) * 8> available_cpus;