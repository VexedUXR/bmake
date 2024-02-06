# $NetBSD: deptgt-posix.mk,v 1.4 2022/05/07 21:24:52 rillig Exp $
#
# Tests for the special target '.POSIX', which enables POSIX mode.
#
# As of 2022-04-18, when parsing the dependency line '.POSIX', the variable
# '%POSIX' is defined and <posix.mk> is included, if it exists.  Other than
# that, POSIX support is still incomplete, the exact set of supported features
# needs to be cross-checked with the POSIX specification.
#
# At the point of '.POSIX:', <sys.mk> has been loaded already, unless the
# option '-r' was given.  This means that an implementation of <posix.mk> must
# work both with and without the system rules from <sys.mk> being in effect.
#
# Implementation note: this test needs to run isolated from the usual tests
# directory to prevent unit-tests/posix.mk from interfering with the posix.mk
# from the system directory that this test uses; since at least 1997, the
# directive '.include <file>' has been looking in the current directory first
# before searching the file in the system search path, as described in
# https://gnats.netbsd.org/15163.
#
# See also:
#	https://pubs.opengroup.org/onlinepubs/9699919799/utilities/make.html

TESTTMP=	${TMPDIR:U${TMP}}\make.test.deptgt-posix
SYSDIR=		${TESTTMP}\sysdir
MAIN_MK=	${TESTTMP}\main.mk
INCLUDED_MK=	${TESTTMP}\included.mk

all: .PHONY
.SILENT:

set-up-sysdir: .USEBEFORE
.if !exists(${SYSDIR})
	mkdir ${SYSDIR}
.endif
	echo CC=sys-cc > ${SYSDIR}\sys.mk
	echo SEEN_SYS_MK=yes >> ${SYSDIR}\sys.mk
	echo CC=posix-cc > ${SYSDIR}/posix.mk

check-is-posix: .USE
	echo .if $${CC} != "posix-cc" >> ${MAIN_MK}
	echo .  error >> ${MAIN_MK}
	echo .endif >> ${MAIN_MK}
	echo .if $${%POSIX} != "1003.2" >> ${MAIN_MK}
	echo .  error >> ${MAIN_MK}
	echo .endif >> ${MAIN_MK}
	echo all: .PHONY >> ${MAIN_MK}

check-not-posix: .USE
	echo .if $${CC} != "sys-cc" >> ${MAIN_MK}
	echo .  error >> ${MAIN_MK}
	echo .endif >> ${MAIN_MK}
	echo .if defined(%POSIX) >> ${MAIN_MK}
	echo .  error >> ${MAIN_MK}
	echo .endif >> ${MAIN_MK}
	echo all: .PHONY >> ${MAIN_MK}

check-not-seen-sys-mk: .USE
	echo .if defined(SEEN_SYS_MK) >> ${MAIN_MK}
	echo .  error >> ${MAIN_MK}
	echo .endif >> ${MAIN_MK}

run: .USE
	(cd "${TESTTMP}" && set MAKEFLAGS=${MAKEFLAGS.${.TARGET}:Q} ${MAKE} \
	    -m "${SYSDIR}" -f ${MAIN_MK:T})
	del /q ${TESTTMP}

# If the main makefile has a '.for' loop as its first non-comment line, a
# strict reading of POSIX 2018 makes the makefile non-conforming.
all: after-for
after-for: .PHONY set-up-sysdir check-not-posix run
	echo # comment > ${MAIN_MK}
	echo: >> ${MAIN_MK}
	echo .for i in once >> ${MAIN_MK}
	echo .POSIX: >> ${MAIN_MK}
	echo .endfor >> ${MAIN_MK}

# If the main makefile has an '.if' conditional as its first non-comment line,
# a strict reading of POSIX 2018 makes the makefile non-conforming.
all: after-if
after-if: .PHONY set-up-sysdir check-not-posix run
	echo # comment > ${MAIN_MK}
	echo: >> ${MAIN_MK}
	echo .if 1 >> ${MAIN_MK}
	echo .POSIX: >> ${MAIN_MK}
	echo .endif >> ${MAIN_MK}

# If the main makefile first includes another makefile and that included
# makefile tries to switch to POSIX mode, that's too late.
all: in-included-file
in-included-file: .PHONY set-up-sysdir check-not-posix run
	echo include included.mk > ${MAIN_MK}
	echo .POSIX: > ${INCLUDED_MK}

# If the main makefile switches to POSIX mode in its very first line, before
# and comment lines or empty lines, that works.
all: in-first-line
in-first-line: .PHONY set-up-sysdir check-is-posix run
	echo .POSIX: > ${MAIN_MK}

# The only allowed lines before switching to POSIX mode are comment lines.
# POSIX defines comment lines as "blank lines, empty lines, and lines with
# <number-sign> ('#') as the first character".
all: after-comment-lines
after-comment-lines: .PHONY set-up-sysdir check-is-posix run
	echo # comment > ${MAIN_MK}
	echo: > ${MAIN_MK}
	echo .POSIX: > ${MAIN_MK}

# Running make with the option '-r' skips the builtin rules from <sys.mk>.
# In that mode, '.POSIX:' just loads <posix.mk>, which works as well.
MAKEFLAGS.no-builtins=	-r
all: no-builtins
no-builtins: .PHONY set-up-sysdir check-is-posix check-not-seen-sys-mk run
	echo .POSIX: > ${MAIN_MK}
