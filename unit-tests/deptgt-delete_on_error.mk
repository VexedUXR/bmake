# $NetBSD: deptgt-delete_on_error.mk,v 1.4 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the special target .DELETE_ON_ERROR in dependency declarations,
# which controls whether the target is deleted if a shell command fails or
# is interrupted.
#
# In compatibility mode, regular and phony targets are deleted, but precious
# targets are preserved.
#
# In parallel mode, regular targets are deleted, while phony and precious
# targets are preserved.
#
# See also:
#	CompatDeleteTarget
#	JobDeleteTarget

THIS=		deptgt-delete_on_error
TARGETS=	${THIS}-regular ${THIS}-regular-delete
TARGETS+=	${THIS}-phony ${THIS}-phony-delete
TARGETS+=	${THIS}-precious ${THIS}-precious-delete

all:
	@echo Compatibility mode
	@-${.MAKE} -f ${MAKEFILE} -k ${TARGETS}
	@del ${TARGETS}
	@echo:
	@echo Parallel mode
	@-${.MAKE} -f ${MAKEFILE} -k -j1 ${TARGETS}
	@del ${TARGETS}

${THIS}-regular{,-delete}:
	type nul > ${.TARGET}& exit 1

${THIS}-phony{,-delete}: .PHONY
	type nul > ${.TARGET}& exit 1

${THIS}-precious{,-delete}: .PRECIOUS
	type nul > ${.TARGET}& exit 1

# The special target .DELETE_ON_ERROR is a global setting.
# It does not apply to single targets.
# The following line is therefore misleading but does not generate any
# warning or even an error message.
.DELETE_ON_ERROR: ${TARGETS:M*-delete}
