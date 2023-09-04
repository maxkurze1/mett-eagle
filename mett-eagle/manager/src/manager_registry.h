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
 * Epiface declaration for the registry interface
 * 
 * @see <l4/mett-eagle/registry>
 */

#pragma once

#include "manager.h"

#include <l4/mett-eagle/registry>

#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_types>

struct Manager_Registry_Epiface
    : L4::Epiface_t<Manager_Registry_Epiface,
                    L4Re::MettEagle::Manager_Registry>
{
  long op_register_client (
      L4Re::MettEagle::Manager_Registry::Rights,
      L4::Ipc::Cap<L4Re::MettEagle::Manager_Client> &manager_ipc_gate);
};