# $NetBSD: depsrc-meta.mk,v 1.7 2022/03/02 19:32:15 sjg Exp $
#
# Tests for the special source .META in dependency declarations.

# TODO: Implementation
# TODO: Explanation

.MAIN: all

.if make(actual-test)
.MAKEFLAGS: -dM
.MAKE.MODE=	meta curDirOk=true
.endif

actual-test: depsrc-meta-target
depsrc-meta-target: .META
	@type nul > ${.TARGET}-file
	@del ${.TARGET}-file

check-results:
	@echo Targets from meta mode${.MAKE.JOBS:D in jobs mode}:
	@findstr TARGET depsrc-meta-target.meta
	@del depsrc-meta-target.meta

all:
	@${MAKE} -r -f ${MAKEFILE} actual-test
	@${MAKE} -r -f ${MAKEFILE} check-results

	@${MAKE} -r -f ${MAKEFILE} actual-test -j1
	@${MAKE} -r -f ${MAKEFILE} check-results -j1
