PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET   = mett-eagle
SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = libloader l4re-util l4re libc_support_misc \
                libpthread cxx_libc_io cxx_io liblog libstdc++

include $(L4DIR)/mk/prog.mk
