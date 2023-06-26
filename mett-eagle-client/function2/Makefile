PKGDIR  ?= ..
L4DIR   ?= $(PKGDIR)/../../..

TARGET   = function2
SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = libfaas liblog

include $(L4DIR)/mk/prog.mk