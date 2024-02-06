# $NetBSD: dep-double-colon-indep.mk,v 1.1 2020/10/23 19:11:30 rillig Exp $
#
# Tests for the :: operator in dependency declarations, which allows multiple
# dependency groups with the same target.  Each group is evaluated on its own,
# independent of the other groups.
#
# This is useful for targets that are updatable, such as a database or a log
# file.  Be careful with parallel mode though, to avoid lost updates and
# other inconsistencies.
#
# The target 1300 depends on 1200, 1400 and 1500.  The target 1200 is older
# than 1300, therefore nothing is done for it.  The other targets are newer
# than 1300, therefore each of them is made, independently from the other.

.END:
	@del dep-double-colon-1*

# Use powershell
p!=	where powershell
.if !exists($p)
.error Cant find powershell
.endif

# We dont need anything else
.SHELL: name=powershell.exe path="${p:S,\\,\\\\,g}" check=%s

_!=	(New-Item \"dep-double-colon-1200\").LastWriteTime=(\"01/01/2020 12:00:00\")
_!=	(New-Item \"dep-double-colon-1300\").LastWriteTime=(\"01/01/2020 13:00:00\")
_!=	(New-Item \"dep-double-colon-1400\").LastWriteTime=(\"01/01/2020 14:00:00\")
_!=	(New-Item \"dep-double-colon-1500\").LastWriteTime=(\"01/01/2020 15:00:00\")

all: dep-double-colon-1300

dep-double-colon-1300:: dep-double-colon-1200
	@echo 'Making 1200 ${.TARGET} from ${.ALLSRC} oodate ${.OODATE}'

dep-double-colon-1300:: dep-double-colon-1400
	@echo 'Making 1400 ${.TARGET} from ${.ALLSRC} oodate ${.OODATE}'

dep-double-colon-1300:: dep-double-colon-1500
	@echo 'Making 1500 ${.TARGET} from ${.ALLSRC} oodate ${.OODATE}'
