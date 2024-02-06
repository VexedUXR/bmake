# $NetBSD: dep-duplicate.mk,v 1.3 2022/01/20 19:24:53 rillig Exp $
#
# Test for a target whose commands are defined twice.  This generates a
# warning, not an error, so ensure that the correct commands are kept.
#
# Also ensure that the diagnostics mention the correct file in case of
# included files.  Since parse.c 1.231 from 2018-12-22 and before parse.c
# 1.653 from 2022-01-20, the wrong filename had been printed if the file of
# the first commands section was included by its relative path.

all: .PHONY
	@echo: > dep-duplicate.main& \
	echo: >> dep-duplicate.main& \
	echo all: >> dep-duplicate.main& \
	echo ^	@echo main-output >> dep-duplicate.main& \
	echo .include "dep-duplicate.inc" >> dep-duplicate.main

	@echo all: > dep-duplicate.inc& \
	echo 	@echo inc-output >> dep-duplicate.inc

	# The main file must be specified using a relative path, just like the
	# default 'makefile' or 'Makefile', to produce the same result when
	# run via ATF or 'make test'.
	@${MAKE} -r -f dep-duplicate.main

	@del dep-duplicate.main
	@del dep-duplicate.inc
