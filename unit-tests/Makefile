# Unit tests for bmake

.MAIN: tests check

#We want delayed expansion. "/v:on"
.SHELL: path=${HOMEDRIVE}\\Windows\\system32\\cmd.exe \
echo="echo %s" ignore="%s" check="%s||exit" separator=& \
meta=\n%&<!>^| escape=^ special=\n,&echo:, args="/v:on /c"

.if ${.OBJDIR} != ${.CURDIR}
TMPDIR:=	${.OBJDIR}\tmp
.else
TMPDIR:=	${TMP}
.endif

.if !exists(${TMPDIR})
!= mkdir ${TMPDIR}
.endif

.include "tests.mk"

UNIT_TESTS:=	${.PARSEDIR}
.PATH: ${UNIT_TESTS}

.if ${USE_ABSOLUTE_TESTNAMES:Uno} == yes
OUTFILES=	${TESTS:@test@${.CURDIR:tA}\\${test}.out@}
.else
OUTFILES=	${TESTS:=.out}
.endif

CLEANFILES=	*.out *.tmp *.diff *.rawout
clean:
	del ${CLEANFILES}

TEST_MAKE?=	${.MAKE}

# We put the changes we want made to the test's
# output in CHANGE, e.g if we wanted to replace
# every 'a' in the output with 'b', we would do
# CHANGE=	a;b. Quotes should be used if spaces
# are present.
# Changes for individual tests can be made with
# CHANGE.test. See tests.mk.

# We dont want absolute paths in the results.
CHANGE+=	'${TEST_MAKE:S,\\,\\\\,g};bmake.exe'
CHANGE+=	'${.CURDIR:S,\\,\\\\,g}\\;'
CHANGE+=	'${.CURDIR:S,\\,\\\\,g};unit-tests'
CHANGE+=	'${.SYSPATH:S,\\,\\\\,g};mk'
CHANGE+=	'${TMPDIR:S,\\,\\\\,g};<tmpdir>'
CHANGE+=	${HOMEDRIVE};<home>

# Default arguments for tests, can be overwritten
# with ARGS.test.
ARGS=	-k

.SUFFIXES: .mk .out .rawout
.mk.rawout:
	@echo Test ${.TARGET:R}& ${ENV.${.PREFIX}:@e@set $e&@} \
	${TEST_MAKE} -r -C${.CURDIR} -f${.IMPSRC} ${ARGS.${.PREFIX}:U${ARGS}} \
	1> ${.TARGET} 2>&1 & echo !errorlevel!>>${.TARGET}

.rawout.out:
	@cscript /nologo replace.vbs ${.IMPSRC} ${.TARGET} \
	${CHANGE.${.PREFIX}:@c@"$c"@} ${CHANGE:@c@"$c"@}

tests: clean ${OUTFILES}

check: ${OUTFILES}
	@echo off& \
	for %i in (${.ALLSRC:R}) do ( \
		fc %i.exp %i.out>%i.diff && \
		echo %i	passed && del %i.out %i.rawout %i.diff  \
		|| (echo %i	FAILED & exit 1) \
	)
	@echo ${TESTS:[#]} tests passed
