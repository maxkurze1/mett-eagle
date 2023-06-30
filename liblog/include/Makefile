PKGDIR ?= ..
L4DIR  ?= $(PKGDIR)/../../..

# find all files in all subdirs - excluding the Makefile
# patsubst to make the path relative again
EXTRA_TARGET += $(patsubst $(SRC_DIR)/%,%,$(shell find $(SRC_DIR) -type f ! -name Makefile))

include $(L4DIR)/mk/include.mk