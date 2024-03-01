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


.if defined(OBJS) && !empty(OBJS)
.NOPATH: ${OBJS} ${PROG}

.c.obj:
	${COMPILE.c} ${.IMPSRC} -o ${.TARGET}

${CXX_SUFFIXES:%=%.obj}:
	${COMPILE.cc} ${.IMPSRC} -o ${.TARGET}

${PROG}.exe: ${OBJS} ${DPADD}
	${_CCLINK} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${_PROGLDOPTS} ${OBJS} ${LDADD}
.else
.c.exe:
	${LINK.c} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}

.endif	# defined(OBJS) && !empty(OBJS)
.endif	# defined(PROG)

.if !defined(_SKIP_BUILD)
realbuild: ${PROG}.exe
.endif

.if !target(clean)
cleanprog:
	del /q \
	    ${PROG}.exe ${OBJS} ${CLEANFILES}

clean: cleanprog
cleandir: cleanprog
.else
cleandir: clean
.endif
.endif
