PKGDIR  = .
L4DIR  ?= $(PKGDIR)/../..

TARGET  = manager worker client include

include $(L4DIR)/mk/subdir.mk
