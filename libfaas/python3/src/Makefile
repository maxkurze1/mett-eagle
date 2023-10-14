PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET   = python-faas3
# MODE     = shared
SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS += libstdc++ libfmt libpython3
# libc_be_fs_noop libc_be_sem_noop
REQUIRES_LIBS += libc_be_sig libc_be_math libc libc_be_l4re libc_be_l4refile libc_support_misc
REQUIRES_LIBS += libdl libpthread
REQUIRES_LIBS += cxx_libc_io cxx_io #cxx_atexit cxx_base cxx_util
# LDFLAGS        += --export-dynamic

include $(L4DIR)/mk/prog.mk