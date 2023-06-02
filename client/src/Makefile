PKGDIR   = ../..
L4DIR   ?= $(PKGDIR)/../..

TARGET   = client
SRC_CC  := $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS   := l4re-util l4re cxx_libc_io cxx_io

include $(L4DIR)/mk/prog.mk