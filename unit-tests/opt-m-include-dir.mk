# $NetBSD: opt-m-include-dir.mk,v 1.4 2020/09/01 20:14:34 rillig Exp $
#
# Tests for the -m command line option, which adds a directory to the
# search path for the .include <...> directive.
#
# The .../canary.mk special argument starts searching in the current
# directory and walks towards the file system root, until it finds a
# directory that contains a file called canary.mk.
#
# To set up this scenario, the file step2.mk is created deep in a hierarchy
# of subdirectories.  Another file called opt-m-step3.mk is created a few
# steps up in the directory hierarchy, serving as the canary file.
#
# Next to the canary file, there is opt-m-step3.mk.  This file is found
# by mentioning its simple name in an .include directive.  It defines the
# target "step2" that is needed by "step2.mk".

.if ${.PARSEFILE:T} == "opt-m-include-dir.mk"

# Set up the other files needed for this test.

TEST_DIR:=	${.PARSEFILE:R}.tmp\sub\sub\sub\workdir
CANARY_FILE:=	${.PARSEFILE:R}.tmp\sub\opt-m-canary.mk
ACTUAL_FILE:=	${.PARSEFILE:R}.tmp\sub\opt-m-step3.mk

_!=	md ${TEST_DIR}
_!=	echo > ${CANARY_FILE}
_!=	copy ${MAKEFILE} ${TEST_DIR}\step2.mk
_!=	copy ${MAKEFILE} ${ACTUAL_FILE}

step1:
	@${.MAKE} -C ${TEST_DIR} -f step2.mk step2

.END:
	@rmdir /s /q ${MAKEFILE:R}.tmp

.elif ${.PARSEFILE:T} == "step2.mk"

# This is the file deep in the directory hierarchy.  It sets up the
# search path for the .include <...> directive and then includes a
# single file from that search path.

# This option adds .tmp/sub to the search path for .include <...>.
.MAKEFLAGS: -m .../opt-m-canary.mk

# This option does not add any directory to the search path since the
# canary file does not exist.
.MAKEFLAGS: -m .../does-not-exist

.include <opt-m-step3.mk>

.elif ${.PARSEFILE:T} == "opt-m-step3.mk"

# This file is included by step2.mk.

step2:
	@echo ok

.else
.  error
.endif
