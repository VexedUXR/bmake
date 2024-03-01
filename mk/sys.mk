# $Id: sys.mk,v 1.57 2023/07/14 16:30:37 sjg Exp $
#
#	@(#) Copyright (c) 2003-2023, Simon J. Gerraty
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

# _DEBUG_MAKE_FLAGS etc.
.include <sys.debug.mk>

.if !empty(_DEBUG_MAKE_FLAGS)
.if ${_DEBUG_MAKE_SYS_DIRS:Uno:@x@${.CURDIR:M$x}@} != ""
.MAKEFLAGS: ${_DEBUG_MAKE_FLAGS}
.endif
.endif

# useful modifiers
.include <sys.vars.mk>


# Enable delayed expansion
.SHELL: path=${HOMEDRIVE}\\Windows\\system32\\cmd.exe \
echo="echo %s" ignore="%s" check="%s||exit" separator=& \
meta=\n%&<!>^| escape=^ special=\n,&echo:, args="/v:on /c"

# we expect a recent bmake
.if !defined(_TARGETS)
# some things we do only once
_TARGETS := ${.TARGETS}
.-include <sys.env.mk>
.endif

# we need HOST_TARGET etc below.
.include <host-target.mk>

# early customizations
.-include <local.sys.env.mk>

# Popular suffixes for C++
CXX_SUFFIXES += .cc .cpp .cxx .C
CXX_SUFFIXES := ${CXX_SUFFIXES:O:u}

SYS_MK ?= ${.PARSEDIR:tA}/${.PARSEFILE}
SYS_MK := ${SYS_MK}

# find the OS specifics
.if defined(SYS_OS_MK)
.include <${SYS_OS_MK}>
.else
_sys_mk =
.for x in ${HOST_TARGET} ${.MAKE.OS} ${.MAKE.OS:S,64,,} ${HOST_OSTYPE} ${MACHINE} Generic
.if empty(_sys_mk)
.-include <sys/$x.mk>
_sys_mk := ${.MAKE.MAKEFILES:M*/$x.mk}
.if !empty(_sys_mk)
_sys_mk := sys/${_sys_mk:T}
.endif
.endif
.if empty(_sys_mk)
# might be an old style
.-include <$x.sys.mk>
_sys_mk := ${.MAKE.MAKEFILES:M*/$x.sys.mk:T}
.endif
.if !empty(_sys_mk)
.break
.endif
.endfor

SYS_OS_MK := ${_sys_mk}
.export SYS_OS_MK
.endif

# some options we need to know early
OPTIONS_DEFAULT_NO += \
	DIRDEPS_BUILD \
	DIRDEPS_CACHE

OPTIONS_DEFAULT_DEPENDENT += \
	AUTO_OBJ/DIRDEPS_BUILD \
	META_MODE/DIRDEPS_BUILD \
	STAGING/DIRDEPS_BUILD \
	STATIC_DIRDEPS_CACHE/DIRDEPS_CACHE \

.include <options.mk>

.if ${MK_DIRDEPS_BUILD} == "yes"
.-include <sys.dirdeps.mk>
.endif
.if ${MK_META_MODE} == "yes"
.-include <meta.sys.mk>
.MAKE.MODE ?= meta verbose {META_MODE}
.endif
# make sure we have a harmless value
.MAKE.MODE ?= normal

# if you want objdirs make them automatic
# and do it early before we compute .PATH
.if ${MK_AUTO_OBJ} == "yes" || ${MKOBJDIRS:Uno} == "auto"
.include <auto.obj.mk>
.endif

.if !empty(SRCTOP)
.if ${.CURDIR} == ${SRCTOP}
RELDIR = .
.elif ${.CURDIR:M${SRCTOP}/*}
RELDIR := ${.CURDIR:S,${SRCTOP}/,,}
.endif
.endif

MACHINE_ARCH.host ?= ${_HOST_ARCH}
MACHINE_ARCH.${MACHINE} ?= ${MACHINE}

MAKE_SHELL ?= ${.SHELL:Ucmd.exe}
SHELL := ${MAKE_SHELL}

# this often helps with debugging
.SUFFIXES:      .cpp-out

.c.cpp-out: .NOTMAIN
	@${COMPILE.c:N-c} -E ${.IMPSRC}

${CXX_SUFFIXES:%=%.cpp-out}: .NOTMAIN
	@${COMPILE.cc:N-c} -E ${.IMPSRC}

# late customizations
.-include <local.sys.mk>

# if .CURDIR is matched by any entry in DEBUG_MAKE_DIRS we
# will apply DEBUG_MAKE_FLAGS, now.
.if !empty(_DEBUG_MAKE_FLAGS)
.if ${_DEBUG_MAKE_DIRS:Uno:@x@${.CURDIR:M$x}@} != ""
.MAKEFLAGS: ${_DEBUG_MAKE_FLAGS}
.endif
.endif
