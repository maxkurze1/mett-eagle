PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET   = liblog

SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = cxx_libc_io cxx_io

include $(L4DIR)/mk/prog.mk
