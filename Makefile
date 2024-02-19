
CFG?=	Debug
ARCH?=	x64
CMD?=	msbuild /p:Configuration=${CFG} /p:Platform=${ARCH}

bmake: .PHONY
	cd msbuild& \
	${CMD}

clean: .PHONY
	cd msbuild& \
	${CMD} /t:Clean
