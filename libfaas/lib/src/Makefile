PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET   = libfaas.a
PC_FILENAME  = libfaas
SRC_CC   = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/*.cc))

REQUIRES_LIBS = libstdc++

include $(L4DIR)/mk/lib.mk