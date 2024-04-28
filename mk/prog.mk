#	$Id: prog.mk,v 1.40 2023/10/02 21:35:43 sjg Exp $

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.include <init.mk>

.SUFFIXES: .obj .c ${CXX_SUFFIXES} .s .exe

CFLAGS+=	${COPTS}

# here is where you can define what LIB* are
.-include <libnames.mk>
.if ${MK_DPADD_MK} == "yes"
# lots of cool magic, but might not suit everyone.
.include <dpadd.mk> # TODO: port this
.endif

.if defined(PROG_CXX)
PROG=		${PROG_CXX}
.endif

.if defined(PROG)
.if empty(SRCS)
# init.mk handling of QUALIFIED_VAR_LIST means
# SRCS will be defined - even if empty.
SRCS = ${PROG}.c
.endif

SRCS ?=	${PROG}.c
OBJS_SRCS = ${SRCS:${OBJS_SRCS_FILTER}}
.for s in ${OBJS_SRCS:M*/*}
${s:T:R}.obj: $s
.endfor
.if !empty(OBJS_SRCS)
OBJS+=	${OBJS_SRCS:T:R:S/$/.obj/g}
.endif

# cl.exe insists on creating intermediate object files,
# so just add them to OBJS to get them cleaned up.
.if ${COMPILER_TYPE} == "msvc" && empty(OBJS)
OBJS=   ${PROG}.obj
.endif

.if defined(OBJS) && !empty(OBJS)
.NOPATH: ${OBJS} ${PROG}

.c.obj:
	${COMPILE.c} ${.IMPSRC} ${CC_OUT}

${CXX_SUFFIXES:%=%.obj}:
	${COMPILE.cc} ${.IMPSRC} ${CC_OUT}

${PROG}.exe: ${OBJS} ${DPADD}
	${_CCLINK} ${LDSTATIC} ${_PROGLDOPTS} ${OBJS} ${LDADD} ${CC_OUT} ${LDFLAGS}
.else
.c.exe:
	${LINK.c} ${.IMPSRC} ${LDLIBS} ${CC_OUT}

.endif	# defined(OBJS) && !empty(OBJS)
.endif	# defined(PROG)

.if !defined(_SKIP_BUILD)
realbuild: ${PROG}.exe
.endif

.if !target(clean)
cleanprog:
	${RM} ${PROG}.exe ${OBJS} ${CLEANFILES}

clean: cleanprog
cleandir: cleanprog
.else
cleandir: clean
.endif
.include <subdir.mk>
.endif
