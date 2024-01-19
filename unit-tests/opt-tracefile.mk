# $NetBSD: opt-tracefile.mk,v 1.5 2021/12/06 22:35:20 rillig Exp $
#
# Tests for the command line option '-T', which in jobs mode appends a trace
# record to a trace log whenever a job is started or completed.

all: .PHONY
	@${MAKE} -f ${MAKEFILE} -j1 -Topt-tracefile.log trace
	@type opt-tracefile.log
	@del opt-tracefile.log

trace dependency1 dependency2: .PHONY
	@echo Making ${.TARGET} from ${.ALLSRC:S,^$,^<nothing^>,W}.

trace: dependency1 dependency2
