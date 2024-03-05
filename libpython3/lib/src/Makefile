PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET      = libpython3.a
PC_FILENAME = libpython3

############################################################################
# This section is copied (with some modification) from the python Makefile #
############################################################################
MODOBJS= \
		Modules/atexitmodule.c \
		Modules/faulthandler.c \
		Modules/posixmodule.c \
		Modules/signalmodule.c \
		Modules/_tracemalloc.c \
		Modules/_codecsmodule.c \
		Modules/_collectionsmodule.c \
		Modules/errnomodule.c \
		Modules/_io/_iomodule.c \
		Modules/_io/iobase.c \
		Modules/_io/fileio.c \
		Modules/_io/bytesio.c \
		Modules/_io/bufferedio.c \
		Modules/_io/textio.c \
		Modules/_io/stringio.c \
		Modules/itertoolsmodule.c \
		Modules/_sre/sre.c \
		Modules/_threadmodule.c \
		Modules/timemodule.c \
		Modules/_weakref.c \
		Modules/_abc.c \
		Modules/_functoolsmodule.c \
		Modules/_localemodule.c \
		Modules/_operator.c \
		Modules/_stat.c \
		Modules/symtablemodule.c \
		Modules/pwdmodule.c \
		Modules/xxsubtype.c

MODULE_OBJS=	\
		Modules/config.c \
		Modules/main.c \
		Modules/gcmodule.c

PEGEN_OBJS=		\
		Parser/pegen.c \
		Parser/pegen_errors.c \
		Parser/action_helpers.c \
		Parser/parser.c \
		Parser/string_parser.c \
		Parser/peg_api.c

POBJS=		\
		Parser/token.c \

PARSER_OBJS=	$(POBJS) $(PEGEN_OBJS) Parser/myreadline.c Parser/tokenizer.c

DYNLOADFILE = dynload_shlib.c
# Python
PYTHON_OBJS=	\
		Python/_warnings.c \
		Python/Python-ast.c \
		Python/Python-tokenize.c \
		Python/asdl.c \
		Python/ast.c \
		Python/ast_opt.c \
		Python/ast_unparse.c \
		Python/bltinmodule.c \
		Python/ceval.c \
		Python/codecs.c \
		Python/compile.c \
		Python/context.c \
		Python/dynamic_annotations.c \
		Python/errors.c \
		Python/frame.c \
		Python/frozenmain.c \
		Python/future.c \
		Python/getargs.c \
		Python/getcompiler.c \
		Python/getcopyright.c \
		Python/getplatform.c \
		Python/getversion.c \
		Python/hamt.c \
		Python/hashtable.c \
		Python/import.c \
		Python/importdl.c \
		Python/initconfig.c \
		Python/marshal.c \
		Python/modsupport.c \
		Python/mysnprintf.c \
		Python/mystrtoul.c \
		Python/pathconfig.c \
		Python/preconfig.c \
		Python/pyarena.c \
		Python/pyctype.c \
		Python/pyfpe.c \
		Python/pyhash.c \
		Python/pylifecycle.c \
		Python/pymath.c \
		Python/pystate.c \
		Python/pythonrun.c \
		Python/pytime.c \
		Python/bootstrap_hash.c \
		Python/specialize.c \
		Python/structmember.c \
		Python/symtable.c \
		Python/sysmodule.c \
		Python/thread.c \
		Python/traceback.c \
		Python/getopt.c \
		Python/pystrcmp.c \
		Python/pystrtod.c \
		Python/pystrhex.c \
		Python/dtoa.c \
		Python/formatter_unicode.c \
		Python/fileutils.c \
		Python/suggestions.c \
		Python/$(DYNLOADFILE)

# Objects
OBJECT_OBJS=	\
		Objects/abstract.c \
		Objects/accu.c \
		Objects/boolobject.c \
		Objects/bytes_methods.c \
		Objects/bytearrayobject.c \
		Objects/bytesobject.c \
		Objects/call.c \
		Objects/capsule.c \
		Objects/cellobject.c \
		Objects/classobject.c \
		Objects/codeobject.c \
		Objects/complexobject.c \
		Objects/descrobject.c \
		Objects/enumobject.c \
		Objects/exceptions.c \
		Objects/genericaliasobject.c \
		Objects/genobject.c \
		Objects/fileobject.c \
		Objects/floatobject.c \
		Objects/frameobject.c \
		Objects/funcobject.c \
		Objects/interpreteridobject.c \
		Objects/iterobject.c \
		Objects/listobject.c \
		Objects/longobject.c \
		Objects/dictobject.c \
		Objects/odictobject.c \
		Objects/memoryobject.c \
		Objects/methodobject.c \
		Objects/moduleobject.c \
		Objects/namespaceobject.c \
		Objects/object.c \
		Objects/obmalloc.c \
		Objects/picklebufobject.c \
		Objects/rangeobject.c \
		Objects/setobject.c \
		Objects/sliceobject.c \
		Objects/structseq.c \
		Objects/tupleobject.c \
		Objects/typeobject.c \
		Objects/unicodeobject.c \
		Objects/unicodectype.c \
		Objects/unionobject.c \
		Objects/weakrefobject.c

DEEPFREEZE_OBJS = Python/deepfreeze/deepfreeze.c

# objects that get linked into the Python library
LIBRARY_OBJS_OMIT_FROZEN=	\
		Modules/getbuildinfo.c \
		$(PARSER_OBJS) \
		$(OBJECT_OBJS) \
		$(PYTHON_OBJS) \
		$(MODULE_OBJS) \
		$(MODOBJS)

LIBRARY_OBJS=	\
		$(LIBRARY_OBJS_OMIT_FROZEN) \
		$(DEEPFREEZE_OBJS) \
		Modules/getpath.c \
		Python/frozen.c

# decimal's libmpdec
# LIBMPDEC_OBJS= \
# 		Modules/_decimal/libmpdec/basearith.c \
# 		Modules/_decimal/libmpdec/constants.c \
# 		Modules/_decimal/libmpdec/context.c \
# 		Modules/_decimal/libmpdec/convolute.c \
# 		Modules/_decimal/libmpdec/crt.c \
# 		Modules/_decimal/libmpdec/difradix2.c \
# 		Modules/_decimal/libmpdec/fnt.c \
# 		Modules/_decimal/libmpdec/fourstep.c \
# 		Modules/_decimal/libmpdec/io.c \
# 		Modules/_decimal/libmpdec/mpalloc.c \
# 		Modules/_decimal/libmpdec/mpdecimal.c \
# 		Modules/_decimal/libmpdec/numbertheory.c \
# 		Modules/_decimal/libmpdec/sixstep.c \
# 		Modules/_decimal/libmpdec/transpose.c
# 		# _decimal does not use signaling API
# 		# Modules/_decimal/libmpdec/mpsignal.c

# pyexpat's expat library
# LIBEXPAT_OBJS= \
# 		Modules/expat/xmlparse.c \
# 		Modules/expat/xmlrole.c \
# 		Modules/expat/xmltok.c

###########################################################################
# end of the copy paste section                                           #
###########################################################################

PRIVATE_INCDIR = $(PKGDIR_ABS)/include/config-headers
# $(PKGDIR_ABS)/include/ARCH-$(ARCH)
# TODO arch headers
PRIVATE_INCDIR += $(PKGDIR)/contrib/Include $(PKGDIR)/contrib/Include/internal $(PKGDIR)/contrib/Python

SRC_C = $(LIBRARY_OBJS)
CFLAGS = -DPy_BUILD_CORE

vpath %.c $(PKGDIR)/contrib

include $(L4DIR)/mk/lib.mk

WARNINGS += \
		-Wno-missing-field-initializers \
		-Wno-missing-declarations \
		-Wno-missing-prototypes \
		-Wno-strict-prototypes \
		-Wno-bad-function-cast \
		-Wno-double-promotion \
		-Wno-unused-parameter \
		-Wno-float-equal \
		-Wno-logical-op