#include <gtest/gtest.h>

#include <l4/mett-eagle/util>
#include <l4/re/env>

#include <string>


TEST (MettEagle, SimpleInvoke)
{
  L4::Cap<L4Re::MettEagle::Manager_Client> manager;
  
  EXPECT_NO_THROW ([&]{ manager = L4Re::MettEagle::getManager ("manager"); });

  EXPECT_NO_THROW ([&]{
    L4Re::chksys (manager->action_create ("test", "example-function"));
  });
  
  std::string answer;
  EXPECT_NO_THROW ([&]{
    L4Re::chksys (manager->action_invoke ("test", answer));
  });

  EXPECT_EQ(answer, std::string("example function answer"));
}

TEST (MettEagle, DoubleUpload)
{
  /**
   * Register the same function twice shouldn't be a problem
   */
  L4::Cap<L4Re::MettEagle::Manager_Client> manager;
  
  EXPECT_NO_THROW ([&]{ manager = L4Re::MettEagle::getManager ("manager"); });

  EXPECT_NO_THROW ([&]{
    L4Re::chksys (manager->action_create ("test1", "example-function"));
  });

  EXPECT_NO_THROW ([&]{
    L4Re::chksys (manager->action_create ("test2", "example-function"));
  });
}

TEST (MettEagle, NameCollision)
{
  /**
   * Should throw an error if the function name is already used
   */
  L4::Cap<L4Re::MettEagle::Manager_Client> manager;
  
  EXPECT_NO_THROW ([&]{ manager = L4Re::MettEagle::getManager ("manager"); });

  EXPECT_NO_THROW ([&]{
    L4Re::chksys (manager->action_create ("some-special-name", "example-function"));
  });

  EXPECT_THROW ([&]{
    L4Re::chksys (manager->action_create ("some-special-name", "example-function"));
  }, L4::Element_already_exists);
}