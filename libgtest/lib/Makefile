PKGDIR ?= ..
L4DIR  ?= $(PKGDIR)/../../..

TARGET   = libgtest.a
PC_FILENAME  = libgtest

PRIVATE_INCDIR = $(PKGDIR_ABS)/contrib/googletest/include $(PKGDIR_ABS)/contrib/googletest

SRC_CC = ../contrib/googletest/src/gtest-all.cc test_runner.cc

# undefine __linux__ to prevent platform specific assumptions
CXXFLAGS = -U __linux__

REQUIRES_LIBS = libstdc++

# surpress compile warnings
WARNINGS :=

include $(L4DIR)/mk/lib.mk