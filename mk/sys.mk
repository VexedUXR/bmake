
CC?=	cl
CXX?=	cl
AS?=	ml
LD?=	link
AR?=	lib

CXX_SUFFIXES+=	.cc .cpp .cxx

# Enable delayed expansion
.SHELL: path=${HOMEDRIVE}\\Windows\\system32\\cmd.exe \
echo="echo %s" ignore="%s" check="%s||exit" separator=& \
meta=\n%&<!>^| escape=^ special=\n,&echo:, args="/v:on /c"

.-include <local.sys.mk>
