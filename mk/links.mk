.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

links: .NOTMAIN
	@echo off& \
	for %l in (${LINKS}) do ( \
		if defined cur ( \
			set "prv=!cur!"& \
			set "cur=%l"& \
			mklink /h !prv! !cur!& \
			set "cur=" \
		) \
		else \
			set "cur=%l" \
	)

links-clean: .NOTMAIN
	@echo off& \
	for %l in (${LINKS}) do \
		if defined cur ( \
			set "cur=" \
		) \
		else \
			del /q %l& \
			set "cur=%l"
.endif
