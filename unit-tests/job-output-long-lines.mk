# $NetBSD: job-output-long-lines.mk,v 1.4 2020/11/01 17:29:13 rillig Exp $
#
# The jobs may produce long lines of output.  A practical case are the echoed
# command lines from compiler invocations, with their many -D options.
#
# Each of these lines must be written atomically to the actual output.
# The markers for switching jobs must always be written at the beginning of
# the line, to make them clearly visible in large log files.
#
# As of 2020-09-27, the default job buffer size is 1024.  When a job produces
# output lines that are longer than this buffer size, these output pieces are
# not terminated by a newline.  Because of this missing newline, the job
# markers "--- job-a ---" and "--- job-b ---" are not always written at the
# beginning of a line, even though this is expected by anyone reading the log
# files.

.MAKEFLAGS: -j2

8000:=	${:U1:S,1,2222222222,g:S,2,3333333333,g:S,3,4444444444,g:S,4,55555555,g}

all: job-a job-b

# Note that the maximum length of a cmd command is 8191 (with /c).
job-a:
	-@echo ${8000:S,5,a,g}

job-b:
	-@echo ${8000:S,5,b,g}