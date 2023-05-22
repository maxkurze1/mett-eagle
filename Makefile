PKGDIR  = .
L4DIR  ?= $(PKGDIR)/../..

TARGET  = manager worker

include $(L4DIR)/mk/subdir.mk
