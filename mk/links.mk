# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: links.mk,v 1.7 2020/08/19 17:51:53 sjg Exp $
#
#	@(#) Copyright (c) 2005, Simon J. Gerraty
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

.if empty(ECHO)
_PIPE=	> nul
.else
_PIPE=
.endif
LINKS?=
SYMLINKS?=

__SYMLINK_SCRIPT= \
	mklink !t! !l! ${_PIPE}

__LINK_SCRIPT= \
	mklink /h !t! !l! ${_PIPE}

_SYMLINKS_SCRIPT= \
	echo off& \
	for %l in (!links!) do ( \
		if defined t ( \
			set "l=!t!"& \
			set "t=${DESTDIR}%l"& \
			${__SYMLINK_SCRIPT}& \
			set "t=" \
		) \
		else \
			set "t=%l" \
	)

_LINKS_SCRIPT= \
	echo off& \
	for %l in (!links!) do ( \
		if defined t ( \
			set "l=!t!"& \
			set "t=${DESTDIR}%l"& \
			${__LINK_SCRIPT}& \
			set "t=" \
		) \
		else \
			set "t=%l" \
	)

_SYMLINKS_USE:	.USE
	@set "links=${$@_SYMLINKS:U${SYMLINKS}}"& ${_SYMLINKS_SCRIPT}

_LINKS_USE:	.USE
	@set "links=${$@_LINKS:U${LINKS}}"& ${_LINKS_SCRIPT}

# sometimes we want to ensure DESTDIR is ignored
_BUILD_SYMLINKS_SCRIPT= \
	echo off& \
	for %l in (!links!) do ( \
		if defined t ( \
			set "l=!t!"& \
			set "t=%l"& \
			${__SYMLINK_SCRIPT}& \
			set "t=" \
		) \
		else \
			set "t=%l" \
	)

_BUILD_LINKS_SCRIPT= \
	echo off& \
	for %l in (!links!) do ( \
		if defined t ( \
			set "l=!t!"& \
			set "t=%l"& \
			${__LINK_SCRIPT}& \
			set "t=" \
		) \
		else \
			set "t=%l" \
	)

_BUILD_SYMLINKS_USE:	.USE
	@set "links=${$@_SYMLINKS:U${SYMLINKS}}"& ${_BUILD_SYMLINKS_SCRIPT}

_BUILD_LINKS_USE:	.USE
	@set "links=${$@_LINKS:U${LINKS}}"& ${_BUILD_LINKS_SCRIPT}
