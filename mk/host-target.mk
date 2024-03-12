# SPDX-License-Identifier: BSD-2-Clause
#
# RCSid:
#	$Id: host-target.mk,v 1.20 2024/02/17 17:26:57 sjg Exp $
#
#	@(#) Copyright (c) 2007-2023 Simon J. Gerraty
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

# Host platform information; may be overridden
.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

.if !defined(_HOST_OSNAME)
# use .MAKE.OS
_HOST_OSNAME := ${.MAKE.OS}
.export _HOST_OSNAME
.endif
.if !defined(_HOST_OSREL)
_HOST_OSREL != systeminfo 2>&1 | findstr /b /c:"OS Version"
_HOST_OSREL := ${_HOST_OSREL:S,OS Version:,,W:M*}
.export _HOST_OSREL
.endif
.if !defined(_HOST_ARCH)
_HOST_ARCH = ${MACHINE_ARCH}
.export _HOST_ARCH
.endif
.if !defined(_HOST_MACHINE)
_HOST_MACHINE = ${MACHINE}
.export _HOST_MACHINE
.endif
.if !defined(HOST_MACHINE)
HOST_MACHINE := ${_HOST_MACHINE}
.export HOST_MACHINE
.endif

HOST_OSMAJOR := ${_HOST_OSREL:C/[^0-9].*//W}
HOST_OSTYPE  :=	${_HOST_OSNAME:S,/,,g}-${_HOST_OSREL}-${_HOST_ARCH}
HOST_OS      :=	${_HOST_OSNAME}
host_os      :=	${_HOST_OSNAME:tl}
HOST_TARGET  := ${host_os}${HOST_OSMAJOR}-${_HOST_ARCH}
# sometimes we want HOST_TARGET32
MACHINE32.amd64 = i386
MACHINE32.x86_64 = i386
.if !defined(_HOST_ARCH32)
_HOST_ARCH32 := ${MACHINE32.${_HOST_ARCH}:U${_HOST_ARCH:S,64$,,}}
.export _HOST_ARCH32
.endif
HOST_TARGET32 := ${host_os:S,/,,g}${HOST_OSMAJOR}-${_HOST_ARCH32}

.export HOST_TARGET HOST_TARGET32
.endif
