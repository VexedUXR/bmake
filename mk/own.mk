# $Id: own.mk,v 1.45 2023/12/30 02:10:38 sjg Exp $

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.if !target(__init.mk__)
.include "init.mk"
.endif

.if !defined(NOMAKECONF) && !defined(NO_MAKECONF)
MAKECONF?=	mk.conf
.-include "${MAKECONF}"
.endif

.include <host-target.mk>

TARGET_OSNAME?= ${_HOST_OSNAME}
TARGET_OSREL?= ${_HOST_OSREL}
TARGET_OSTYPE?= ${HOST_OSTYPE}
TARGET_HOST?= ${HOST_TARGET}

# these may or may not exist
.-include <${TARGET_HOST}.mk>
.-include <config.mk>

# we need to make sure these are defined too in case sys.mk fails to.
COMPILE.s?=	${CC} ${AFLAGS} -c
LINK.s?=	${CC} ${AFLAGS} ${LDFLAGS}
COMPILE.S?=	${CC} ${AFLAGS} ${CPPFLAGS} -c -traditional-cpp
LINK.S?=	${CC} ${AFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.c?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.c?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}
CXXFLAGS?=	${CFLAGS}
COMPILE.cc?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} -c
LINK.cc?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.m?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} -c
LINK.m?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.f?=	${FC} ${FFLAGS} -c
LINK.f?=	${FC} ${FFLAGS} ${LDFLAGS}
COMPILE.F?=	${FC} ${FFLAGS} ${CPPFLAGS} -c
LINK.F?=	${FC} ${FFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.r?=	${FC} ${FFLAGS} ${RFLAGS} -c
LINK.r?=	${FC} ${FFLAGS} ${RFLAGS} ${LDFLAGS}
LEX.l?=		${LEX} ${LFLAGS}
COMPILE.p?=	${PC} ${PFLAGS} ${CPPFLAGS} -c
LINK.p?=	${PC} ${PFLAGS} ${CPPFLAGS} ${LDFLAGS}
YACC.y?=	${YACC} ${YFLAGS}

# for suffix rules
IMPFLAGS?=	${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}}
.for s in .c .cc
COMPILE.$s += ${IMPFLAGS}
LINK.$s +=  ${IMPFLAGS}
.endfor

# we really like to have SRCTOP and OBJTOP defined...
.if !defined(SRCTOP) || !defined(OBJTOP)
.-include <srctop.mk>
.endif

.if !defined(SRCTOP) || !defined(OBJTOP)
# dpadd.mk is rather pointless without these
OPTIONS_DEFAULT_NO+= DPADD_MK
.endif

# process options
OPTIONS_DEFAULT_NO+= \
	PROG_LDORDER_MK \

OPTIONS_DEFAULT_YES+= \
	ARCHIVE \
	AUTODEP \
	DPADD_MK \
	GDB \
	LINKLIB \
	OBJ \

OPTIONS_DEFAULT_DEPENDENT+= \
	LDORDER_MK/PROG_LDORDER_MK \
	OBJDIRS/OBJ \
	PROFILE/LINKLIB \

.include <options.mk>

# allow for per target flags
# apply the :T:R first, so the more specific :T can override if needed
CPPFLAGS += ${CPPFLAGS_${.TARGET:T:R}} ${CPPFLAGS_${.TARGET:T}}
CFLAGS += ${CFLAGS_${.TARGET:T:R}} ${CFLAGS_${.TARGET:T}}

.if ${MK_OBJ} == "no"
MK_OBJDIRS=	no
MK_AUTO_OBJ=	no
.endif

# :U incase not using our sys.mk
.if ${MK_META_MODE:Uno} == "yes"
# should all be set by sys.mk if not default
TARGET_SPEC_VARS ?= MACHINE
.if ${TARGET_SPEC_VARS:[#]} > 1
TARGET_SPEC_VARS_REV := ${TARGET_SPEC_VARS:[-1..1]}
.else
TARGET_SPEC_VARS_REV = ${TARGET_SPEC_VARS}
.endif
.endif

.endif
