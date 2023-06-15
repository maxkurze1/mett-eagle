PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

# pkgdir '?' makes a difference????

# the name of the library
TARGET   = liblog.a

SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = cxx_libc_io cxx_io

include $(L4DIR)/mk/lib.mk
