# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: init.mk,v 1.37 2024/02/25 19:12:13 sjg Exp $
#	@(#) Copyright (c) 2002, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

_this_mk_dir := ${.PARSEDIR:tA}

.-include <local.init.mk>
.-include <${.CURDIR:H}/Makefile.inc>
.include <own.mk>
.include <compiler.mk>

.MAIN:		all

# should have been set by sys.mk
CXX_SUFFIXES ?= .cc .cpp .cxx .C

# SRCS which do not end up in OBJS
NO_OBJS_SRCS_SUFFIXES ?= .h
OBJS_SRCS_FILTER += ${NO_OBJS_SRCS_SUFFIXES:@x@N*$x@:ts:}

.if defined(PROG_CXX) || ${SRCS:Uno:${CXX_SUFFIXES:S,^,N*,:ts:}} != ${SRCS:Uno}
_CCLINK ?=	${CXX}
.endif
_CCLINK ?=	${CC}

# these are applied in order, least specific to most
VAR_QUALIFIER_LIST += \
	${TARGET_SPEC_VARS:UMACHINE:@v@${$v}@} \
	${COMPILER_TYPE} \
	${.TARGET:T:R} \
	${.TARGET:T} \
	${.IMPSRC:T} \
	${VAR_QUALIFIER_XTRA_LIST}

QUALIFIED_VAR_LIST += \
	CFLAGS \
	COPTS \
	CPPFLAGS \
	CPUFLAGS \
	LDFLAGS \
	SRCS \

# a final :U avoids errors if someone uses :=
.for V in ${QUALIFIED_VAR_LIST:O:u:@q@$q $q_LAST@}
.for Q in ${VAR_QUALIFIER_LIST:u}
$V += ${$V_$Q:U${$V.$Q:U}} ${V_$Q_${COMPILER_TYPE}:U${$V.$Q.${COMPILER_TYPE}:U}}
.endfor
.endfor

# del is unnecessarily verbose for
# what we need.
RM?=	del /q > nul 2>&1

.if ${COMPILER_TYPE} == "msvc"
# cl.exe wants us to pass some arguments
# to the linker instead of to it.
CC_OUT?=	/link /out:${.TARGET}

CFLAGS+=	/nologo
LDFLAGS+=	/nologo
.else
CC_OUT?=	-o ${.TARGET}
.endif

.if ${.MAKE.LEVEL:U1} == 0 && ${MK_DIRDEPS_BUILD:Uno} == "yes"
.if ${RELDIR} == "."
# top-level targets that are ok at level 0
DIRDEPS_BUILD_LEVEL0_TARGETS += clean* destroy*
M_ListToSkip?= O:u:S,^,N,:ts:
.if ${.TARGETS:Uall:${DIRDEPS_BUILD_LEVEL0_TARGETS:${M_ListToSkip}}} != ""
# this tells lib.mk and prog.mk to not actually build anything
_SKIP_BUILD = not building at level 0
.endif
.elif ${.TARGETS:U:Nall} == ""
_SKIP_BUILD = not building at level 0
.endif
.endif

.if !defined(.PARSEDIR)
# no-op is the best we can do if not bmake.
.WAIT:
.endif

# define this once for consistency
.if !defined(_SKIP_BUILD)
# beforebuild is a hook for things that must be done early
all: beforebuild .WAIT realbuild
.else
all: .PHONY
.if !empty(_SKIP_BUILD) && ${.MAKEFLAGS:M-V} == ""
.warning ${_SKIP_BUILD}
.endif
.endif
beforebuild:
realbuild:

.endif
