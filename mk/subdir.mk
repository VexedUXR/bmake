#	$Id: subdir.mk,v 1.16 2017/02/08 22:16:59 sjg Exp $
#	skip missing directories...

#	$NetBSD: bsd.subdir.mk,v 1.11 1996/04/04 02:05:06 jtc Exp $
#	@(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91

.if !commands(_SUBDIRUSE) && !defined(NO_SUBDIR) && !defined(NOSUBDIR)
.-include <${.CURDIR}\Makefile.inc>

ECHO_DIR ?= echo
.ifdef SUBDIR_MUST_EXIST
MISSING_DIR=echo Missing ===^> ${.CURDIR}\%e& exit 1
.else
MISSING_DIR=echo Skipping ===^> ${.CURDIR}\%e& set "_continue_=yes"
.endif

_SUBDIRUSE: .USE
.if defined(SUBDIR)
	@echo off& \
	for %e in (${SUBDIR}) do ( \
		( \
			set "_continue_=no"& \
			if exist ${.CURDIR}\%e.${MACHINE} ( \
				set "_newdir_=%e.${MACHINE}" \
			) \
			else ( \
				if exist ${.CURDIR}\%e\Makefile ( \
					set "_newdir_=%e" \
				) \
				else ( \
					${MISSING_DIR} \
				) \
			) \
		)& \
		if [!_continue_!] == [no] ( \
			( \
				if defined _THISDIR_ ( \
					set "_nextdir_=!_THISDIR_!\!_newdir_!" \
				) \
				else ( \
					set "_nextdir_=!_newdir_!" \
				) \
			)& \
			${ECHO_DIR} ===^> !_nextdir_!& \
			cd ${.CURDIR}\!_newdir_!& \
			${.MAKE} _THISDIR_="!_nextdir_!" ${.TARGET}|| \
			exit !errorlevel! \
		) \
	)

${SUBDIR}::
	@( \
		if exist ${.CURDIR}\${.TARGET}.${MACHINE} ( \
			set "_newdir_=${.TARGET}.${MACHINE}" \
		) \
		else ( \
			set "_newdir_=${.TARGET}" \
		) \
	)& \
	${ECHO_DIR} ===^> !_newdir_!& \
	cd ${.CURDIR}\!_newdir_!&& \
	${.MAKE} _THISDIR_="!_newdir_!" all
.endif

SUBDIR_TARGETS += \
	all \
	clean \
	cleandir \

.for t in ${SUBDIR_TARGETS:O:u}
$t: _SUBDIRUSE
.endfor

.include <own.mk>
.endif
# make sure this exists
all:
