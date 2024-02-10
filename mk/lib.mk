.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

.SUFFIXES: .c ${CXX_SUFFIXES} .obj

_EXT=	.c ${CXX_SUFFIXES}
OBJS+=	${SRCS:U${dir /b ${_EXT:@S@${LIB}$S@}:L:sh}:@O@${O:R}.obj@}

CFLAGS+=	/nologo
CFLAGS+=	${COPTS}

${_EXT:@S@$S.obj@}:
	${CC} /c "${.IMPSRC}" ${CFLAGS} /Fo"${.TARGET}"

${LIB}: ${DPADD} ${LIB}.lib .PHONY
${LIB}.lib: ${OBJS}
	${AR} /nologo ${OBJS} /out:${LIBDIR}${.TARGET}

clean: .PHONY
	del /q ${OBJS} ${LIB}.lib
.endif
