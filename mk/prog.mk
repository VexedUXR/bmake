#	$Id: prog.mk,v 1.40 2023/10/02 21:35:43 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

.include <init.mk>

.SUFFIXES: .obj .c .cc .C .s .exe

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
SRCS?=	${PROG}.c
.for s in ${SRCS:N*.h:M*/*}
${s:T:R}.obj: $s
.endfor
.if !empty(SRCS:N*.h)
OBJS+=	${SRCS:T:N*.h:R:S/$/.obj/g}
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
	${_CCLINK} ${LDFLAGS} ${LDSTATIC} ${_PROGLDOPTS} ${OBJS} ${LDADD} ${CC_OUT}
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
.endif
