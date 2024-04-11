PROG=	bmake

SUBDIR+=	tre
LDADD+=		user32.lib tre.lib
LDFLAGS+=	/libpath:tre

CFLAGS+=	\
/D USE_META	\
/D HAVE_REGEX_H	\
/D MAKE_VERSION=\"20240404\" \
/MT \
/W3 \
/wd4996

.if !make(deploy)
CFLAGS+=	-Od
.else
CFLAGS+=	-O2 -Ot

deploy: realbuild
.endif
SRCS=		\
arch.c		\
buf.c		\
compat.c	\
cond.c		\
dir.c		\
dirname.c	\
for.c		\
hash.c		\
job.c		\
lst.c		\
main.c		\
make.c		\
make_malloc.c	\
message.c	\
meta.c		\
parse.c		\
str.c		\
stresep.c	\
strlcpy.c	\
suff.c		\
targ.c		\
trace.c		\
util.c		\
var.c		\

.include <prog.mk>
