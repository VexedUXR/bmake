# $NetBSD: opt-x-reduce-exported.mk,v 1.3 2022/05/08 07:27:50 rillig Exp $
#
# Tests for the -X command line option, which prevents variables passed on the
# command line from being exported to the environment of child commands.

# The variable 'BEFORE' is exported, the variable 'AFTER' isn't.
.MAKEFLAGS: BEFORE=before -X AFTER=after

all: .PHONY ordinary submake

ordinary: .PHONY
	@echo ordinary:
	@set | findstr /B "BEFORE AFTER"

submake: .PHONY
	@echo submake:
	@${MAKE} -r -f ${MAKEFILE} show-env

show-env: .PHONY
	@set | findstr /B "BEFORE AFTER"
