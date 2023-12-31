/*	$NetBSD: trace.c,v 1.33 2023/03/28 14:39:31 rillig Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * trace.c --
 *	handle logging of trace events generated by various parts of make.
 *
 * Interface:
 *	Trace_Init		Initialize tracing (called once during
 *				the lifetime of the process)
 *
 *	Trace_End		Finalize tracing (called before make exits)
 *
 *	Trace_Log		Log an event about a particular make job.
 */

#include <process.h>
#include <sys/timeb.h>

#include "make.h"
#include "job.h"
#include "trace.h"

static FILE *trfile;
static int trpid;
static const char *trwd;

static const char evname[][4] = {
	"BEG",
	"END",
	"ERR",
	"JOB",
	"DON",
	"INT",
};

void
Trace_Init(const char *pathname)
{
	if (pathname != NULL) {
		FStr curDir;
		trpid = _getpid();
		/*
		 * XXX: This variable may get overwritten later, which would
		 * make trwd point to undefined behavior.
		 */
		curDir = Var_Value(SCOPE_GLOBAL, ".CURDIR");
		trwd = curDir.str;

		trfile = fopen(pathname, "a");
	}
}

void
Trace_Log(TrEvent event, Job *job)
{
	struct _timeb rightnow;

	if (trfile == NULL)
		return;

	_ftime(&rightnow);

	fprintf(trfile, "%lld.%03ld %d %s %d %s",
	    (long long)rightnow.time, (long)rightnow.millitm,
	    jobTokensRunning,
	    evname[event], trpid, trwd);

	if (job != NULL) {
		char flags[4];

		Job_FlagsToString(job, flags, sizeof flags);
		fprintf(trfile, " %s %lu %s %x", job->node->name,
		    job->pid, flags, job->node->type);
	}
	fputc('\n', trfile);
	fflush(trfile);
}

void
Trace_End(void)
{
	if (trfile != NULL)
		fclose(trfile);
}
