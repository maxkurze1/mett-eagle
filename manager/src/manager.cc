#include <l4/ned/cmd_control>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <stdio.h>
#include <unistd.h>

int
main (void)
{
  auto ned = L4Re::chkcap (
      L4Re::Env::env ()->get_cap<L4Re::Ned::Cmd_control> ("ned"),
      "Couldn't find ned!");
  L4::Ipc::String<> s (
      "local L4 = require(\"L4\");L4.default_loader:start({}, \"rom/worker "
      "some_arg\");");
  ned->execute (s);

  for (int i = 0; i < 10; i++)
    {
      ned->execute (s);
    }
}