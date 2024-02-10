.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

.SUFFIXES: .c ${CXX_SUFFIXES} .obj

_PROG=	${PROG_CXX:U${PROG}}
_CC=	${PROG_CXX:D${CXX}:U${CC}}
_EXT=	${PROG_CXX:D${CXX_SUFFIXES}:U.c}
OBJS+=	${SRCS:U${dir /b ${_EXT:@S@${_PROG}$S@}:L:sh}:@O@${O:R}.obj@}

CFLAGS+=	/nologo
CFLAGS+=	${COPTS}
LDFLAGS+=	/nologo

${_EXT:@S@$S.obj@}:
	${_CC} /c "${.IMPSRC}" ${CFLAGS} /Fo"${.TARGET}"

${_PROG}: ${DPADD} ${_PROG}.exe links .PHONY
${_PROG}.exe: ${OBJS}
	${LD} ${OBJS} ${LDFLAGS} /out:"${.TARGET}"

clean: links-clean .PHONY
	del /q ${OBJS} ${PROG}.exe

.include <links.mk>
.endif
