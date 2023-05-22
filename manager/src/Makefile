PKGDIR   = ../..
L4DIR   ?= $(PKGDIR)/../..

TARGET   = manager
SRC_CC  := $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS   := libloader l4re-util l4re lua++ libc_support_misc \
                   libpthread cxx_libc_io cxx_io

include $(L4DIR)/mk/prog.mk
