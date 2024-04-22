PROG=	bmake

SUBDIR+=	tre
LDADD+=		user32.lib tre.lib
LDFLAGS+=	/libpath:tre

# Version
CFLAGS+=	/D MAKE_VERSION=\"20240404\"

# Disable some warnings

# Disable warning when size_t is converted to a smaller type.
# arguably we can avoid this using a cast.
# Used in LoadFile in parse.c
NOWARN+= 4267
# This disables a warning we get when using the '-' operator on an
# unsigned type.
# Used in TryParseNumber in parse.c
NOWARN+= 4146
# Disable the deprecated function warning, which triggers on alot
# of the standard libc functions.
# Used all over the place.
NOWARN+= 4996

CFLAGS+=	${NOWARN:@w@/wd$w@}

CFLAGS+=	\
/D USE_META	\
/D HAVE_REGEX_H	\
/MT \
/W3


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
var.c

.if !make(release)
CFLAGS+=	/Od
.else
CFLAGS+=	/O2 /Ot

release: realbuild
.endif

.include <prog.mk>
