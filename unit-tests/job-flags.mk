# $NetBSD: job-flags.mk,v 1.2 2020/11/14 13:17:47 rillig Exp $
#
# Tests for Job.flags, which are controlled by special source dependencies
# like .SILENT or .IGNORE, as well as the command line options -s or -i.

.MAKEFLAGS: -j1

all: silent .WAIT ignore .WAIT ignore-cmds

.BEGIN:
	@echo $@

silent: .SILENT .PHONY
	echo $@

ignore: .IGNORE .PHONY
	@echo $@
	@echo exit 0 in $@
	@cmd /c exit 0 in $@
	@echo exit 1 in $@
	@cmd /c exit 1 in $@
	@echo 'Still there in $@'

ignore-cmds: .PHONY
	# This node is not marked .IGNORE; individual commands can be switched
	# to ignore mode by prefixing them with a '-'.
	#-false without indentation
	@echo exit 1 without indentation
	-@cmd /c exit 1
	# This also works if the '-' is indented by a space or a tab.
	# Leading whitespace is stripped off by ParseLine_ShellCommand.
	# -false space
	#	-false tab
	@echo exit 1 space
	 -@cmd /c exit 1
	@echo exit 1 tab
		-@cmd /c exit 1

.END:
	@echo $@
