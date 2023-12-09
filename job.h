/*	$NetBSD: job.h,v 1.77 2021/12/15 12:58:01 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)job.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)job.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Running of jobs in parallel mode.
 */

#ifndef MAKE_JOB_H
#define MAKE_JOB_H

#include <winsock2.h>
#undef SP_ERROR
#undef SearchPath
#undef ERROR

#ifdef USE_META
# include "meta.h"
#endif

typedef enum JobStatus {
	JOB_ST_FREE	= 0,	/* Job is available */
	JOB_ST_SET_UP	= 1,	/* Job is allocated but otherwise invalid */
	/* XXX: What about the 2? */
	JOB_ST_RUNNING	= 3,	/* Job is running, pid valid */
	JOB_ST_FINISHED	= 4	/* Job is done (ie after SIGCHILD) */
} JobStatus;

/*
 * A Job manages the shell commands that are run to create a single target.
 * Each job is run in a separate subprocess by a shell.  Several jobs can run
 * in parallel.
 *
 * The shell commands for the target are written to a temporary file,
 * then the shell is run with the temporary file as stdin, and the output
 * of that shell is captured via a pipe.
 *
 * When a job is finished, Make_Update updates all parents of the node
 * that was just remade, marking them as ready to be made next if all
 * other dependencies are finished as well.
 */
typedef struct Job {
	/* The process ID of the shell running the commands */
	DWORD pid;

	/* The process handle of the shell running the commands */
	HANDLE handle;

	/* The target the child is making */
	GNode *node;

	/*
	 * If one of the shell commands is "...", all following commands are
	 * delayed until the .END node is made.  This list node points to the
	 * first of these commands, if any.
	 */
	StringListNode *tailCmds;

	/* This is where the shell commands go. */
	Buffer *cmdBuffer;

	DWORD exit_status;

	JobStatus status;

	/* Ignore non-zero exits */
	bool ignerr;
	/* Output the command before or instead of running it. */
	bool echo;
	/* Target is a special one. */
	bool special;

	HANDLE inPipe;		/* Pipe for reading output from job */
	HANDLE outPipe;		/* Pipe for writing control commands */

#define JOB_BUFSIZE	1024
	/* Buffer for storing the output of the job, line by line. */
	char outBuf[JOB_BUFSIZE + 1];
	size_t curPos;		/* Current position in outBuf. */

#ifdef USE_META
	struct BuildMon bm;
#endif
} Job;

extern const char *shellPath;
extern const char *shellName;
extern char *shellErrFlag;

extern int jobTokensRunning;	/* tokens currently "out" */

void Shell_Init(void);
const char *Shell_GetNewline(void) MAKE_ATTR_USE;
void Job_Touch(GNode *, bool);
bool Job_CheckCommands(GNode *, void (*abortProc)(const char *, ...))
	MAKE_ATTR_USE;
void Job_CatchChildren(void);
void Job_Make(GNode *);
void Job_Init(void);
bool Job_ParseShell(char *) MAKE_ATTR_USE;
int Job_Finish(void);
void Job_End(void);
void Job_Wait(void);
void Job_AbortAll(void);
void Job_TokenReturn(void);
bool Job_TokenWithdraw(void) MAKE_ATTR_USE;
void Job_ServerStart(int, HANDLE, HANDLE);
void Job_SetPrefix(void);
bool Job_RunTarget(const char *, const char *);
void Job_FlagsToString(const Job *, char *, size_t);
FILE *Job_TempFile(const char *, char *, size_t) MAKE_ATTR_USE;
void Job_WaitMutex(void);
void Job_ReleaseMutex(void);

#endif
