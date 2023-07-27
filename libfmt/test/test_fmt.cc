#include <l4/fmt/core.h>

#include <gtest/gtest.h>

TEST(libfmt, fmt_format) {
  EXPECT_EQ(fmt::format("{} {}", "Hello", "World"), std::string("Hello World"));
  EXPECT_EQ(fmt::format("{:s} {:s}", "Hello", "World"), std::string("Hello World"));
}