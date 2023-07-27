PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

# the name of the library
TARGET   = libfmt.a
# PC_FILENAME = libfmt

SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

PRIVATE_INCDIR = $(PKGDIR_ABS)/include
CXXFLAGS       += -std=gnu++20 -fmodules-ts
REQUIRES_LIBS = libstdc++

# surpress compile warnings
WARNINGS =

include $(L4DIR)/mk/lib.mk
