# Gtest

This is a port of google's test [library](https://github.com/google/googletest).

The contrib directory contains a copy of the 1.13.0 release.

## Test Anything Protocol (TAP)

This port also contains a sightly modified version of the
[GTest TAP Listener](https://github.com/kinow/gtest-tap-listener) which can be
accessed as `<gtest/tap/tap.h>`.

TAP is used to interact with the custom makefile based build system 'BID'.

## Usage

The gtest library and the tap listener are both included in the build library
binary. This library can be can be linked to compiled test files and directly
executed by the build system.

To create such a test:

1.  make a `test` directory (thats only convention)
2.  inside this directory, create a Makefile for the tests, here an example:

        PKGDIR ?= ..
        L4DIR  ?= $(PKGDIR)/../..

        REQUIRES_LIBS += libgtest

        # surpress compile warnings
        WARNINGS =

        include $(L4DIR)/mk/test.mk

3.  create a c++ file that will contain your tests. This file should be prefixed
    with 'test\_' to be automatically recognized by the Makefile. Your test file
    can look like this:

        #include <gtest/gtest.h>

        TEST(ExampleGroup, ExampleTest) {
          EXPECT_EQ(1, 1);
        }

4.  you also should add `requires: libgtest` to your package 'Control' file

Now you should be able to call `make test` and see the result of your test case.

## Further information

Please refer to the official [gtest](https://google.github.io/googletest/) documentation for more information.
