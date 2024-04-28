# $Id: lib.mk,v 1.81 2023/10/03 18:18:57 sjg Exp $

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.include <init.mk>

SHLIB_FULLVERSION ?= ${${SHLIB_MAJOR} ${SHLIB_MINOR} ${SHLIB_TEENY}:L:ts.}
SHLIB_FULLVERSION := ${SHLIB_FULLVERSION}

# add additional suffixes not exported.
# .po is used for profiling object files.
.SUFFIXES: .out .lib .obj .s .S .c .h ${CXX_SUFFIXES}

CFLAGS+=	${COPTS}

META_NOECHO?= echo

# we simplify life by letting the toolchain do most of the work
# _CCLINK is set by init.mk based on whether we are doing C++ or not
SHLIB_LD ?= ${_CCLINK}

.if ${COMPILER_TYPE} == "msvc"
LD_shared ?= /dll
.else
LD_shared ?= -shared
.endif

.if ${MK_LINKLIB} != "no" && !empty(SHLIB_MAJOR) && empty(SHLIB_LINKS)
SHLIB_LINKS = ${LIB}.${SHLIB_MAJOR}.dll
.if "${SHLIB_FULLVERSION}" != "${SHLIB_MAJOR}"
SHLIB_LINKS += ${LIB}.${SHLIB_FULLVERSION}.dll
.endif
.endif

.c.obj:
	${COMPILE.c} ${.IMPSRC} ${CC_OUT}

${CXX_SUFFIXES:%=%.obj}:
	${COMPILE.cc} ${.IMPSRC} ${CC_OUT}

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

OBJS_SRCS = ${SRCS:${OBJS_SRCS_FILTER}}

.for s in ${SRCS:N*.h:M*/*}
${s:T:R}.obj: $s
.endfor

OBJS+=	${OBJS_SRCS:T:R:S/$/.obj/g}
.NOPATH:	${OBJS}

${LIB}.lib: ${OBJS}
	@${META_NOECHO} building standard ${LIB} library
	@${RM} ${.TARGET}
	lib /nologo /OUT:${.TARGET} ${OBJS}

# cl uses the /link argument to pass arguments to the linker
# *everything* after it is passed to the linker.
_ := ${${COMPILER_TYPE} == "msvc":?/link:}
${LIB}.dll: ${LIB}.lib ${DPADD}
	@${META_NOECHO} building shared ${LIB} library (version ${SHLIB_FULLVERSION})
	@${RM} ${.TARGET}
	${SHLIB_LD} ${OBJS} ${LDADD} ${SHLIB_LDADD} \
	    $_ ${LDFLAGS:N$_} ${LD_shared} ${CC_OUT:N$_}
.if !empty(SHLIB_LINKS)
	${RM} ${SHLIB_LINKS} & ${SHLIB_LINKS:O:u:@x@mklink $x ${.TARGET}&@}
.endif

.if !target(clean)
cleanlib: .PHONY
	-${RM} ${CLEANFILES}
	${RM} ${LIB}.lib ${OBJS}
	${RM} ${LIB}.dll
.if !empty(SHLIB_LINKS)
	${RM} ${SHLIB_LINKS}
.endif

clean: cleanlib
cleandir: cleanlib
.else
cleandir: clean
.endif
.include <subdir.mk>
.endif
