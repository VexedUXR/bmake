# $NetBSD: opt-chdir.mk,v 1.6 2021/05/18 17:05:45 sjg Exp $
#
# Tests for the -C command line option, which changes the directory at the
# beginning.
#
# This option has been available since 2009-08-27.

.MAKEFLAGS: -d0			# un-buffer stdout

all: chdir-filename-too-long
all: chdir-root
all: chdir-nonexistent

# Try to overflow the internal buffer for .CURDIR, which is curdir.
chdir-filename-too-long: .PHONY .IGNORE
	# 130 slashes, separated by dots: /./././.../././
	# note that we are limited by MAX_PATH
	@${MAKE} -C ${:U:range=130:@@/@:ts.}

# Changing to another directory is possible via the command line.
# In this test, it is the root directory since almost any other directory
# is not guaranteed to exist on every platform.
chdir-root: .PHONY .IGNORE
	@set MAKE_OBJDIR_CHECK_WRITABLE=no & ${MAKE} -C ${HOMEDRIVE}\ -V "cwd: $${.CURDIR}"

# Trying to change to a nonexistent directory exits immediately.
# Note: just because the whole point of /nonexistent is that it should
# not exist - doesn't mean it doesn't.
chdir-nonexistent: .PHONY .IGNORE
	@${MAKE} -C /nonexistent
