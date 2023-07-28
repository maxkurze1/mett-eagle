PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET   = python-faas
MODE     = shared
SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = libstdc++ libfmt libpython libc_be_sig libc_be_math cxx_libc_io cxx_io
# LDFLAGS        += --export-dynamic

include $(L4DIR)/mk/prog.mk