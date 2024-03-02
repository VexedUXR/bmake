# $Id: compiler.mk,v 1.12 2023/10/03 18:47:48 sjg Exp $
#
#	@(#) Copyright (c) 2019, Simon J. Gerraty
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

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

CC?=	cl
CXX?=	${CC}

.if empty(COMPILER_TYPE) || empty(COMPILER_VERSION)
.if ${CC:R} == "cl"
_v != cl 2>&1
COMPILER_TYPE = msvc
.else
# gcc does not always say gcc
_v != (${CC} --version) 2> nul | \
	findstr /i "clang gcc Free^ Software^ Foundation"
.endif
.if empty(COMPILER_TYPE)
.if ${_v:Mclang} != ""
COMPILER_TYPE = clang
.elif ${_v:M[Gg][Cc][Cc]} != "" || ${_v:MFoundation*} != "" || ${CC:T:M*gcc*} != ""
COMPILER_TYPE = gcc
.endif
.endif
.if empty(COMPILER_VERSION)
COMPILER_VERSION := ${_v:M[1-9][0-9]*.[0-9]*}
.endif
.undef _v
.endif
# just in case we don't recognize compiler
COMPILER_TYPE ?= unknown
COMPILER_VERSION ?= 0
.endif
