# $Id: lib.mk,v 1.81 2023/10/03 18:18:57 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

.include <init.mk>

SHLIB_FULLVERSION ?= ${${SHLIB_MAJOR} ${SHLIB_MINOR} ${SHLIB_TEENY}:L:ts.}
SHLIB_FULLVERSION := ${SHLIB_FULLVERSION}

# add additional suffixes not exported.
# .po is used for profiling object files.
.SUFFIXES: .out .lib .obj .s .S .c .cc .C .h

CFLAGS+=	${COPTS}

META_NOECHO?= echo

# we simplify life by letting the toolchain do most of the work
# _CCLINK is set by init.mk based on whether we are doing C++ or not
SHLIB_LD ?= ${_CCLINK}
LD_shared ?= -shared

.if ${MK_LINKLIB} != "no" && !empty(SHLIB_MAJOR) && empty(SHLIB_LINKS)
SHLIB_LINKS = ${LIB}.${SHLIB_MAJOR}.dll
.if "${SHLIB_FULLVERSION}" != "${SHLIB_MAJOR}"
SHLIB_LINKS += ${LIB}.${SHLIB_FULLVERSION}.dll
.endif
.endif

.c.obj:
	${COMPILE.c} ${.IMPSRC} -o ${.TARGET}

${CXX_SUFFIXES:%=%.obj}:
	${COMPILE.cc} ${.IMPSRC} -o ${.TARGET}

.S.obj .s.obj:
	${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC}

_LIBS=
.if ${MK_ARCHIVE} != "no"
_LIBS += ${LIB}.lib
.endif
.if defined(SHLIB_MAJOR)
_LIBS += ${LIB}.dll
.endif

# here is where you can define what LIB* are
.-include <libnames.mk>
.if ${MK_DPADD_MK} == "yes"
# lots of cool magic, but might not suit everyone.
.include <dpadd.mk>
.endif

.if empty(LIB)
_LIBS=
.endif

.if !defined(_SKIP_BUILD)
realbuild: ${_LIBS}
.endif

.for s in ${SRCS:N*.h:M*/*}
${s:T:R}.obj: $s
.endfor

OBJS+=	${SRCS:T:N*.h:R:S/$/.obj/g}
.NOPATH:	${OBJS}

${LIB}.lib: ${OBJS}
	@${META_NOECHO} building standard ${LIB} library
	@del /q ${.TARGET} 2> nul
	lib /nologo /OUT:${.TARGET} ${OBJS}

${LIB}.dll: ${LIB}.lib ${DPADD}
	@${META_NOECHO} building shared ${LIB} library (version ${SHLIB_FULLVERSION})
	@del /q ${.TARGET} 2> nul
	${SHLIB_LD} ${OBJS} ${LDFLAGS} -o ${.TARGET} \
	    ${LD_shared} ${LDADD} ${SHLIB_LDADD}
.if !empty(SHLIB_LINKS)
	del /q ${SHLIB_LINKS} & ${SHLIB_LINKS:O:u:@x@mklink $x ${.TARGET}&@}
.endif

.if !target(clean)
cleanlib: .PHONY
	-del /q ${CLEANFILES}
	-del /q ${LIB}.lib ${OBJS}
	-del /q ${LIB}.dll
.if !empty(SHLIB_LINKS)
	-del /q ${SHLIB_LINKS}
.endif

clean: cleanlib
cleandir: cleanlib
.else
cleandir: clean
.endif
.endif
