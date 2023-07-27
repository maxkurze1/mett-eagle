PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../../..

TARGET   = example-function
SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = libfaas

include $(L4DIR)/mk/prog.mk