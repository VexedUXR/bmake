/*	$NetBSD: job.c,v 1.459 2023/02/15 06:52:58 rillig Exp $	*/

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
 */

/*
 * job.c --
 *	handle the creation etc. of our child processes.
 *
 * Interface:
 *	Job_Init	Called to initialize this module. In addition,
 *			the .BEGIN target is made including all of its
 *			dependencies before this function returns.
 *			Hence, the makefiles must have been parsed
 *			before this function is called.
 *
 *	Job_End		Clean up any memory used.
 *
 *	Job_Make	Start the creation of the given target.
 *
 *	Job_CatchChildren
 *			Check for and handle the termination of any
 *			children. This must be called reasonably
 *			frequently to keep the whole make going at
 *			a decent clip, since job table entries aren't
 *			removed until their process is caught this way.
 *
 *	Job_CatchOutput
 *			Print any output our children have produced.
 *			Should also be called fairly frequently to
 *			keep the user informed of what's going on.
 *			If no output is waiting, it will block for
 *			a time given by the SEL_* constants, below,
 *			or until output is ready.
 *
 *	Job_ParseShell	Given a special dependency line with target '.SHELL',
 *			define the shell that is used for the creation
 *			commands in jobs mode.
 *
 *	Job_Finish	Perform any final processing which needs doing.
 *			This includes the execution of any commands
 *			which have been/were attached to the .END
 *			target. It should only be called when the
 *			job table is empty.
 *
 *	Job_AbortAll	Abort all currently running jobs. Do not handle
 *			output or do anything for the jobs, just kill them.
 *			Should only be called in an emergency.
 *
 *	Job_CheckCommands
 *			Verify that the commands for a target are
 *			ok. Provide them if necessary and possible.
 *
 *	Job_Touch	Update a target without really updating it.
 *
 *	Job_Wait	Wait for all currently-running jobs to finish.
 */

#include <sys/utime.h>
#include <process.h>
#include <memory.h>
#include <io.h>

#include "make.h"
#include "dir.h"
#include "job.h"
#include "pathnames.h"
#include "trace.h"

/*	"@(#)job.c	8.2 (Berkeley) 3/19/94"	*/

/*
 * A shell defines how the commands are run.  All commands for a target are
 * written into a single file, which is then given to the shell to execute
 * the commands from it.  The commands are written to the file using a few
 * templates for echo control and error control.
 *
 * The name of the shell is the basename for the predefined shells, such as
 * "sh", "csh", "bash".  For custom shells, it is the full pathname, and its
 * basename is used to select the type of shell; the longest match wins.
 * So /usr/pkg/bin/bash has type sh, /usr/local/bin/tcsh has type csh.
 *
 * The echoing of command lines is controlled using hasEchoCtl, echoOff,
 * echoOn, noPrint and noPrintLen.  When echoOff is executed by the shell, it
 * still outputs something, but this something is not interesting, therefore
 * it is filtered out using noPrint and noPrintLen.
 *
 * The error checking for individual commands is controlled using hasErrCtl,
 * errOn, errOff and runChkTmpl.
 *
 * In case a shell doesn't have error control, echoTmpl is a printf template
 * for echoing the command, should echoing be on; runIgnTmpl is another
 * printf template for executing the command while ignoring the return
 * status. Finally runChkTmpl is a printf template for running the command and
 * causing the shell to exit on error. If any of these strings are empty when
 * hasErrCtl is false, the command will be executed anyway as is, and if it
 * causes an error, so be it. Any templates set up to echo the command will
 * escape any '$ ` \ "' characters in the command string to avoid unwanted
 * shell code injection, the escaped command is safe to use in double quotes.
 *
 * The command-line flags "echo" and "exit" also control the behavior.  The
 * "echo" flag causes the shell to start echoing commands right away.  The
 * "exit" flag causes the shell to exit when an error is detected in one of
 * the commands.
 */
typedef struct Shell {
	const char *name;	/* name of the shell */

	/* template to run a command without error checking */
	const char *runIgnTmpl;
	/* template to run a command with error checking */
	const char *runChkTmpl;
	const char *echoTmpl;	/* template to echo a command */

	/*
	 * Character used to execute multiple commands on one line,
	 * regardless of whether the last command failed or not.
	 */
	char separator;
	char commentChar; /* character used by shell for comment lines */
} Shell;

typedef struct CommandFlags {
	/* Whether to echo the command before or instead of running it. */
	bool echo;

	/* Run the command even in -n or -N mode. */
	bool always;

	/*
	 * true if we turned error checking off before writing the command to
	 * the commands file and need to turn it back on
	 */
	bool ignerr;
} CommandFlags;

/*
 * Write shell commands to a file.
 *
 * TODO: keep track of whether commands are echoed.
 * TODO: keep track of whether error checking is active.
 */
typedef struct ShellWriter {
	Buffer *b;

	/* we've sent 'set -x' */
	bool xtraced;

} ShellWriter;

/*
 * error handling variables
 */
static int job_errors = 0;	/* number of errors reported */
static enum {			/* Why is the make aborting? */
	ABORT_NONE,
	ABORT_ERROR,		/* Aborted because of an error */
	ABORT_INTERRUPT,	/* Aborted because it was interrupted */
	ABORT_WAIT		/* Waiting for jobs to finish */
} aborting = ABORT_NONE;
#define JOB_TOKENS "+EI+"	/* Token to requeue for each abort state */

/*
 * this tracks the number of tokens currently "out" to build jobs.
 */
int jobTokensRunning = 0;

typedef enum JobStartResult {
	JOB_RUNNING,		/* Job is running */
	JOB_ERROR,		/* Error in starting the job */
	JOB_FINISHED		/* The job is already finished */
} JobStartResult;

/*
 * Descriptions for various shells.
 *
 * The build environment may set DEFSHELL_INDEX to one of
 * DEFSHELL_INDEX_SH, DEFSHELL_INDEX_KSH, or DEFSHELL_INDEX_CSH, to
 * select one of the predefined shells as the default shell.
 *
 * Alternatively, the build environment may set DEFSHELL_CUSTOM to the
 * name or the full path of a sh-compatible shell, which will be used as
 * the default shell.
 *
 * ".SHELL" lines in Makefiles can choose the default shell from the
 * set defined here, or add additional shells.
 */

#ifdef DEFSHELL_CUSTOM
#define DEFSHELL_INDEX_CUSTOM 0
#endif /* !DEFSHELL_CUSTOM */

#ifndef DEFSHELL_INDEX
#define DEFSHELL_INDEX 0	/* DEFSHELL_INDEX_CUSTOM or DEFSHELL_INDEX_SH */
#endif /* !DEFSHELL_INDEX */

static Shell shells[] = {
	/* Command Prompt description.*/
	{
		"cmd.exe", /* .name */
		"%s\n", /* .runIgnTmpl */
		"%s||exit !errorlevel!\n", /* .runChkTmpl */
		"echo %s\n", /* .echoTmpl */
		'&', /* .separator */
		'\0' /* .commentChar */
	},
	/* Powershell description. */
	{
		"pwsh.exe", /* .name */

		/*
		 * We set $lastexitcode to null if the command fails
		 * in order not to confuse runChkTmpl.
		 */
		"$(%s)||$($lastexitcode=$null)\n", /* .runIgnTmpl */

		/*
		 * Powershell only sets $lastexitcode when
		 * and actual application is run, so it wont
		 * be set if a command is misspelt for example.
		 * In this case, we simply exit with 1.
		 */
		"$(%s)||$(if($lastexitcode-ne$null)"
		"{exit $lastexitcode}exit 1)\n", /* .runChkTmpl */

		"$(echo \"\"%s\"\")\n", /* .echoTmpl */
		';', /* .separator */
		'#', /* .commentChar */
	}
};

/*
 * This is the shell to which we pass all commands in the Makefile.
 * It is set by the Job_ParseShell function.
 */
static Shell *shell = &shells[DEFSHELL_INDEX];
const char *shellPath = NULL;	/* full pathname of executable image */
const char *shellName = NULL;	/* last component of shellPath */
char *shellErrFlag = NULL;
static char *shell_freeIt = NULL; /* Allocated memory for custom .SHELL */


static Job *job_table;		/* The structures that describe them */
static Job *job_table_end;	/* job_table + maxJobs */

static char *targPrefix = NULL;	/* To identify a job change in the output. */
static Job tokenWaitJob;	/* token wait pseudo-job */

/*
 * Windows' signal() supports 6 signals.
 * We only use 2.
 */
#define SIGMAX 2

static int caught_signals[SIGMAX]; /* Set of signals we handle */
static SignalProc signal_handlers[SIGMAX]; /* Signal handlers for the signals we handle */
static int pending_signals[SIGMAX];

static void CollectOutput(Job *, bool);
static void MAKE_ATTR_DEAD JobInterrupt(bool, int);
static void JobSigReset(void);

static void
SwitchOutputTo(GNode *gn)
{
	/* The node for which output was most recently produced. */
	static GNode *lastNode = NULL;

	if (gn == lastNode)
		return;
	lastNode = gn;

	if (opts.maxJobs != 1 && targPrefix != NULL && targPrefix[0] != '\0')
		(void)fprintf(stdout, "%s %s ---\n", targPrefix, gn->name);
}

void
Job_FlagsToString(const Job *job, char *buf, size_t bufsize)
{
	snprintf(buf, bufsize, "%c%c%c",
	    job->ignerr ? 'i' : '-',
	    !job->echo ? 's' : '-',
	    job->special ? 'S' : '-');
}

static void
DumpJobs(const char *where)
{
	Job *job;
	char flags[4];

	debug_printf("job table @ %s\n", where);
	for (job = job_table; job < job_table_end; job++) {
		Job_FlagsToString(job, flags, sizeof flags);
		debug_printf("job %d, status %d, flags %s, pid %lu\n",
			(int)(job - job_table), job->status, flags, job->pid);
	}
}

/*
 * Delete the target of a failed, interrupted, or otherwise
 * unsuccessful job unless inhibited by .PRECIOUS.
 */
static void
JobDeleteTarget(GNode *gn)
{
	const char *file;

	if (gn->type & OP_JOIN)
		return;
	if (gn->type & OP_PHONY)
		return;
	if (GNode_IsPrecious(gn))
		return;
	if (opts.noExecute)
		return;

	file = GNode_Path(gn);
	if (unlink_file(file) == 0)
		Error("*** %s removed", file);
}

static void
JobSigPending(int sig)
{
	int i;

	/* If sig is already pending, dont add it again. */
	for (i = 0; i < SIGMAX; i++) {
		if (pending_signals[i] == NSIG)
			break;
		else if (pending_signals[i] == sig)
			return;
	}

	pending_signals[i] = sig;
}

/*
 * JobSigLock/JobSigUnlock
 *
 * Signal lock routines to get exclusive access. Currently used to
 * protect `jobs' and `stoppedJobs' list manipulations.
 * 
 * Windows dosent have sigprocmask(), so we emulate the behaviour of
 * SIG_BLOCK by passing all signals to JobSigPending which puts the signal
 * in pending_signals where it can later be processed.
 */
static void
JobSigLock(void)
{
	for (int i = 0; i < SIGMAX; i++) {
		if (caught_signals[i] != NSIG)
			(void)signal(caught_signals[i], JobSigPending);
		/* This discards any previous pending signals. */
		pending_signals[i] = NSIG;
	}
}

/* Process pending signals. */
static void
JobSigUnlock(void)
{
	for (int i = 0; i < SIGMAX; i++) {
		if (caught_signals[i] != NSIG)
			signal(caught_signals[i], signal_handlers[i]);
		
		if (pending_signals[i] != NSIG) {
			int j = 0;
			for (; caught_signals[j] != pending_signals[i]; j++);
			signal_handlers[j](caught_signals[j]);
		}
	}
}

static void
JobCreatePipe(Job *job)
{
	if (CreatePipe(&job->inPipe, &job->outPipe, NULL, 0) == 0)
		Punt("failed to create pipe: %s", strerr(GetLastError()));
	/*
	 * We mark the input side of the pipe non-blocking; we might lose the
	 * race for the token when a new one becomes available, so the read
	 * from the pipe should not block.
	 */
	if (SetNamedPipeHandleState(job->inPipe, &(DWORD){PIPE_NOWAIT},
		NULL, NULL) == 0)
		Punt("failed to set pipe handle state: %s", strerr(GetLastError()));
	if (SetHandleInformation(job->outPipe, HANDLE_FLAG_INHERIT,
		HANDLE_FLAG_INHERIT) == 0)
		Punt("failed to set pipe attributes: %s", strerr(GetLastError()));
}

/* Pass the signal to each running job. */
static void
JobCondPassSig(int signo)
{
	Job *job;

	DEBUG1(JOB, "JobCondPassSig(%d) called.\n", signo);

	for (job = job_table; job < job_table_end; job++) {
		if (job->status != JOB_ST_RUNNING)
			continue;
		DEBUG2(JOB, "JobCondPassSig passing signal %d to child %lu.\n",
			signo, job->pid);
		TerminateProcess(job->handle, signo);
	}
}

/*
 * Pass a signal on to all jobs, then resend to ourselves.
 * We die by the same signal.
 */
MAKE_ATTR_DEAD static void
JobPassSig_int(int signo)
{
	/* Run .INTERRUPT target then exit */
	JobInterrupt(true, signo);
}

/*
 * Pass a signal on to all jobs, then resend to ourselves.
 * We die by the same signal.
 */
MAKE_ATTR_DEAD static void
JobPassSig_term(int signo)
{
	/* Dont run .INTERRUPT target then exit */
	JobInterrupt(false, signo);
}

static Job *
JobFindHandle(HANDLE handle, JobStatus status, bool isJobs)
{
	Job *job;

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == status && job->handle == handle)
			return job;
	}
	if (DEBUG(JOB) && isJobs)
		DumpJobs("no handle");
	return NULL;
}

/* Parse leading '@', '-' and '+', which control the exact execution mode. */
static void
ParseCommandFlags(char **pp, CommandFlags *out_cmdFlags)
{
	char *p = *pp;
	out_cmdFlags->echo = true;
	out_cmdFlags->ignerr = false;
	out_cmdFlags->always = false;

	for (;;) {
		if (*p == '@')
			out_cmdFlags->echo = DEBUG(LOUD);
		else if (*p == '-')
			out_cmdFlags->ignerr = true;
		else if (*p == '+')
			out_cmdFlags->always = true;
		else if (!ch_isspace(*p))
			/* Ignore whitespace for compatibility with gnu make */
			break;
		p++;
	}

	pp_skip_whitespace(&p);

	*pp = p;
}

/* Escape a string for a double-quoted string literal in sh, csh and ksh. */
static char *
EscapeShellDblQuot(const char *cmd)
{
	size_t i, j;

	/* Worst that could happen is every char needs escaping. */
	char *esc = bmake_malloc(strlen(cmd) * 2 + 1);
	for (i = 0, j = 0; cmd[i] != '\0'; i++, j++) {
		if (cmd[i] == '$' || cmd[i] == '`' || cmd[i] == '\\' ||
		    cmd[i] == '"')
			esc[j++] = '\\';
		esc[j] = cmd[i];
	}
	esc[j] = '\0';

	return esc;
}

static void
ShellWriter_WriteFmt(ShellWriter *wr, const char *fmt, const char *arg)
{
	DEBUG1(JOB, fmt, arg);

	char *str = _alloca(snprintf(NULL, 0, fmt, arg) + 1);
	sprintf(str, fmt, arg);

	Buf_AddStr(wr->b, str);
}

static void
ShellWriter_WriteLine(ShellWriter *wr, const char *line)
{
	ShellWriter_WriteFmt(wr, "%s\n", line);
}

static void
ShellWriter_EchoCmd(ShellWriter *wr, const char *escCmd)
{
	ShellWriter_WriteFmt(wr, shell->echoTmpl, escCmd);
}

static void
ShellWriter_TraceOn(ShellWriter *wr)
{
	if (!wr->xtraced) {
		ShellWriter_WriteLine(wr, "set -x");
		wr->xtraced = true;
	}
}

/*
 * The shell has no built-in error control, so emulate error control by
 * enclosing each shell command in a template like "{ %s \n } || exit $?"
 * (configurable per shell).
 */
static void
JobWriteSpecialsEchoCtl(Job *job, ShellWriter *wr, CommandFlags *inout_cmdFlags,
			const char *escCmd, const char **inout_cmdTemplate)
{
	/* XXX: Why is the whole job modified at this point? */
	job->ignerr = true;

	if (job->echo && inout_cmdFlags->echo) {
		ShellWriter_EchoCmd(wr, escCmd);

		/*
		 * Leave echoing off so the user doesn't see the commands
		 * for toggling the error checking.
		 */
		inout_cmdFlags->echo = false;
	}
	*inout_cmdTemplate = shell->runIgnTmpl;

	/*
	 * The template runIgnTmpl already takes care of ignoring errors,
	 * so pretend error checking is still on.
	 * XXX: What effects does this have, and why is it necessary?
	 */
	inout_cmdFlags->ignerr = false;
}

static void
JobWriteSpecials(Job *job, ShellWriter *wr, const char *escCmd, bool run,
		 CommandFlags *inout_cmdFlags, const char **inout_cmdTemplate)
{
	if (!run) {
		/*
		 * If there is no command to run, there is no need to switch
		 * error checking off and on again for nothing.
		 */
		inout_cmdFlags->ignerr = false;
	}  else if (shell->runIgnTmpl != NULL && shell->runIgnTmpl[0] != '\0') {
		JobWriteSpecialsEchoCtl(job, wr, inout_cmdFlags, escCmd,
		    inout_cmdTemplate);
	} else
		inout_cmdFlags->ignerr = false;
}

/*
 * Write a shell command to the job's commands file, to be run later.
 *
 * If the command starts with '@' and neither the -s nor the -n flag was
 * given to make, stick a shell-specific echoOff command in the script.
 *
 * If the command starts with '-' and the shell has no error control (none
 * of the predefined shells has that), ignore errors for the entire job.
 *
 * XXX: Why ignore errors for the entire job?  This is even documented in the
 * manual page, but without any rationale since there is no known rationale.
 *
 * XXX: The manual page says the '-' "affects the entire job", but that's not
 * accurate.  The '-' does not affect the commands before the '-'.
 *
 * If the command is just "...", skip all further commands of this job.  These
 * commands are attached to the .END node instead and will be run by
 * Job_Finish after all other targets have been made.
 */
static void
JobWriteCommand(Job *job, ShellWriter *wr, StringListNode *ln, const char *ucmd)
{
	bool run;

	CommandFlags cmdFlags;
	/* Template for writing a command to the shell file */
	const char *cmdTemplate;
	char *xcmd;		/* The expanded command */
	char *xcmdStart;
	char *escCmd;		/* xcmd escaped to be used in double quotes */

	run = GNode_ShouldExecute(job->node);

	xcmd = Var_Subst(ucmd, job->node, VARE_WANTRES);
	/* TODO: handle errors */
	xcmdStart = xcmd;

	cmdTemplate = "%s\n";

	ParseCommandFlags(&xcmd, &cmdFlags);

	/* The '+' command flag overrides the -n or -N options. */
	if (cmdFlags.always && !run) {
		/*
		 * We're not actually executing anything...
		 * but this one needs to be - use compat mode just for it.
		 */
		(void)Compat_RunCommand(ucmd, job->node, ln);
		free(xcmdStart);
		return;
	}

	/*
	 * If the shell doesn't have error control, the alternate echoing
	 * will be done (to avoid showing additional error checking code)
	 * and this needs some characters escaped.
	 */
	escCmd = xcmd;
	
	if (cmdFlags.ignerr) {
		JobWriteSpecials(job, wr, escCmd, run, &cmdFlags, &cmdTemplate);
	} else {

		/*
		 * If errors are being checked and the shell doesn't have
		 * error control but does supply an runChkTmpl template, then
		 * set up commands to run through it.
		 */

		if (shell ->runChkTmpl != NULL &&
		    shell->runChkTmpl[0] != '\0') {
			if (job->echo && cmdFlags.echo) {
				ShellWriter_EchoCmd(wr, escCmd);
				cmdFlags.echo = false;
			}
			/*
			 * If it's a comment line or blank, avoid the possible
			 * syntax error generated by "{\n} || exit $?".
			 */
			cmdTemplate = escCmd[0] == shell->commentChar ||
				      escCmd[0] == '\0'
			    ? shell->runIgnTmpl
			    : shell->runChkTmpl;
			cmdFlags.ignerr = false;
		}
	}

	/* XXX: Not needed? */
	if (DEBUG(SHELL) && strcmp(shellName, "sh") == 0)
		ShellWriter_TraceOn(wr);

	ShellWriter_WriteFmt(wr, cmdTemplate, xcmd);
	free(xcmdStart);
}

/*
 * Write all commands to the shell file that is later executed.
 *
 * The special command "..." stops writing and saves the remaining commands
 * to be executed later, when the target '.END' is made.
 *
 * Return whether at least one command was written to the shell file.
 */
static bool
JobWriteCommands(Job *job)
{
	StringListNode *ln;
	bool seen = false;
	ShellWriter wr;

	wr.b = job->cmdBuffer;
	wr.xtraced = false;

	for (ln = job->node->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;

		if (strcmp(cmd, "...") == 0) {
			job->node->type |= OP_SAVE_CMDS;
			job->tailCmds = ln->next;
			break;
		}

		JobWriteCommand(job, &wr, ln, ln->datum);
		seen = true;
	}

	return seen;
}

/*
 * Save the delayed commands (those after '...'), to be executed later in
 * the '.END' node, when everything else is done.
 */
static void
JobSaveCommands(Job *job)
{
	StringListNode *ln;

	for (ln = job->tailCmds; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;
		char *expanded_cmd;
		/*
		 * XXX: This Var_Subst is only intended to expand the dynamic
		 * variables such as .TARGET, .IMPSRC.  It is not intended to
		 * expand the other variables as well; see deptgt-end.mk.
		 */
		expanded_cmd = Var_Subst(cmd, job->node, VARE_WANTRES);
		/* TODO: handle errors */
		Lst_Append(&Targ_GetEndNode()->commands, expanded_cmd);
	}
}


/* Called to close both input and output pipes when a job is finished. */
static void
JobClosePipes(Job *job)
{
	CollectOutput(job, true);

	CloseHandle(job->outPipe);
	job->outPipe = NULL;

	CloseHandle(job->inPipe);
	job->inPipe = NULL;
}

static void
DebugFailedJob(const Job *job)
{
	const StringListNode *ln;

	if (!DEBUG(ERROR))
		return;

	debug_printf("\n");
	debug_printf("*** Failed target: %s\n", job->node->name);
	debug_printf("*** Failed commands:\n");
	for (ln = job->node->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;
		debug_printf("\t%s\n", cmd);

		if (strchr(cmd, '$') != NULL) {
			char *xcmd = Var_Subst(cmd, job->node, VARE_WANTRES);
			debug_printf("\t=> %s\n", xcmd);
			free(xcmd);
		}
	}
}

static void
JobFinishDoneExitedError(Job *job, DWORD *inout_status)
{
	SwitchOutputTo(job->node);
#ifdef USE_META
	if (useMeta) {
		meta_job_error(job, job->node,
		    job->ignerr, *inout_status);
	}
#endif
	if (!shouldDieQuietly(job->node, -1)) {
		DebugFailedJob(job);
		(void)printf("*** [%s] Error code %ld%s\n",
		    job->node->name, *inout_status,
		    job->ignerr ? " (ignored)" : "");
	}

	if (job->ignerr)
		*inout_status = 0;
	else {
		if (deleteOnError)
			JobDeleteTarget(job->node);
		PrintOnError(job->node, "\n");
	}
}

static void
JobFinishDoneExited(Job *job, DWORD *inout_status)
{
	DEBUG2(JOB, "Process %lu [%s] exited.\n", job->pid, job->node->name);

	if (*inout_status != 0)
		JobFinishDoneExitedError(job, inout_status);
	else if (DEBUG(JOB)) {
		SwitchOutputTo(job->node);
		(void)printf("*** [%s] Completed successfully\n",
		    job->node->name);
	}
}

/*
 * Do final processing for the given job including updating parent nodes and
 * starting new jobs as available/necessary.
 *
 * Deferred commands for the job are placed on the .END node.
 *
 * If there was a serious error (job_errors != 0; not an ignored one), no more
 * jobs will be started.
 *
 * Input:
 *	job		job to finish
 *	status		sub-why job went away
 */
static void
JobFinish(Job *job, DWORD status)
{
	bool return_job_token;

	DEBUG3(JOB, "JobFinish: %lu [%s], status %lu\n",
	    job->pid, job->node->name, status);

	JobClosePipes(job);
	CloseHandle(job->handle);
	if (job->cmdBuffer != NULL) {
		Buf_Done(job->cmdBuffer);

		free(job->cmdBuffer);
		job->cmdBuffer = NULL;
	}

	if (status != 0)
		JobFinishDoneExited(job, &status);

#ifdef USE_META
	if (useMeta) {
		int meta_status = meta_job_finish(job);
		if (meta_status != 0 && status == 0)
			status = meta_status;
	}
#endif

	return_job_token = false;

	Trace_Log(JOBEND, job);
	if (!job->special) {
		if (status != 0 ||
		    (aborting == ABORT_ERROR) || aborting == ABORT_INTERRUPT)
			return_job_token = true;
	}

	if (aborting != ABORT_ERROR && aborting != ABORT_INTERRUPT &&
	    status == 0) {
		/*
		 * As long as we aren't aborting and the job didn't return a
		 * non-zero status that we shouldn't ignore, we call
		 * Make_Update to update the parents.
		 */
		JobSaveCommands(job);
		job->node->made = MADE;
		if (!job->special)
			return_job_token = true;
		Make_Update(job->node);
		job->status = JOB_ST_FREE;
	} else if (status != 0) {
		job_errors++;
		job->status = JOB_ST_FREE;
	}

	if (job_errors > 0 && !opts.keepgoing && aborting != ABORT_INTERRUPT) {
		/* Prevent more jobs from getting started. */
		aborting = ABORT_ERROR;
	}

	if (return_job_token)
		Job_TokenReturn();

	if (aborting == ABORT_ERROR && jobTokensRunning == 0)
		Finish(job_errors);
}

static void
TouchRegular(GNode *gn)
{
	const char *file = GNode_Path(gn);
	ULARGE_INTEGER t;
	FILETIME ft;
	HANDLE fh;

	/* convert our time_t value to a FILETIME value */
	t.QuadPart = (now * 10000000LL) + 116444736000000000LL;
	ft.dwLowDateTime = t.LowPart;
	ft.dwHighDateTime = t.HighPart;

	if ((fh = CreateFileA(file, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		Punt("failed to create/open file \"%s\": %s", file,
			strerr(GetLastError()));
	if (SetFileTime(fh, NULL, &ft, &ft) == 0)
		Punt("failed to set file time: %s", strerr(GetLastError()));

	CloseHandle(fh);
}

/*
 * Touch the given target. Called by JobStart when the -t flag was given.
 *
 * The modification date of the file is changed.
 * If the file did not exist, it is created.
 */
void
Job_Touch(GNode *gn, bool echo)
{
	if (gn->type &
	    (OP_JOIN | OP_USE | OP_USEBEFORE | OP_EXEC | OP_OPTIONAL |
	     OP_SPECIAL | OP_PHONY)) {
		/*
		 * These are "virtual" targets and should not really be
		 * created.
		 */
		return;
	}

	if (echo || !GNode_ShouldExecute(gn)) {
		(void)fprintf(stdout, "touch %s\n", gn->name);
		(void)fflush(stdout);
	}

	if (!GNode_ShouldExecute(gn))
		return;

	if (gn->type & OP_ARCHV)
		Arch_Touch(gn);
	else if (gn->type & OP_LIB)
		Arch_TouchLib(gn);
	else
		TouchRegular(gn);
}

/*
 * Make sure the given node has all the commands it needs.
 *
 * The node will have commands from the .DEFAULT rule added to it if it
 * needs them.
 *
 * Input:
 *	gn		The target whose commands need verifying
 *	abortProc	Function to abort with message
 *
 * Results:
 *	true if the commands list is/was ok.
 */
bool
Job_CheckCommands(GNode *gn, void (*abortProc)(const char *, ...))
{
	if (GNode_IsTarget(gn))
		return true;
	if (!Lst_IsEmpty(&gn->commands))
		return true;
	if ((gn->type & OP_LIB) && !Lst_IsEmpty(&gn->children))
		return true;

	/*
	 * No commands. Look for .DEFAULT rule from which we might infer
	 * commands.
	 */
	if (defaultNode != NULL && !Lst_IsEmpty(&defaultNode->commands) &&
	    !(gn->type & OP_SPECIAL)) {
		/*
		 * The traditional Make only looks for a .DEFAULT if the node
		 * was never the target of an operator, so that's what we do
		 * too.
		 *
		 * The .DEFAULT node acts like a transformation rule, in that
		 * gn also inherits any attributes or sources attached to
		 * .DEFAULT itself.
		 */
		Make_HandleUse(defaultNode, gn);
		Var_Set(gn, IMPSRC, GNode_VarTarget(gn));
		return true;
	}

	Dir_UpdateMTime(gn, false);
	if (gn->mtime != 0 || (gn->type & OP_SPECIAL))
		return true;

	/*
	 * The node wasn't the target of an operator.  We have no .DEFAULT
	 * rule to go on and the target doesn't already exist. There's
	 * nothing more we can do for this branch. If the -k flag wasn't
	 * given, we stop in our tracks, otherwise we just don't update
	 * this node's parents so they never get examined.
	 */

	if (gn->flags.fromDepend) {
		if (!Job_RunTarget(".STALE", gn->fname))
			fprintf(stdout,
			    "%s: %s, %u: ignoring stale %s for %s\n",
			    progname, gn->fname, gn->lineno, makeDependfile,
			    gn->name);
		return true;
	}

	if (gn->type & OP_OPTIONAL) {
		(void)fprintf(stdout, "%s: don't know how to make %s (%s)\n",
		    progname, gn->name, "ignored");
		(void)fflush(stdout);
		return true;
	}

	if (opts.keepgoing) {
		(void)fprintf(stdout, "%s: don't know how to make %s (%s)\n",
		    progname, gn->name, "continuing");
		(void)fflush(stdout);
		return false;
	}

	abortProc("%s: don't know how to make %s. Stop", progname, gn->name);
	return false;
}

/*
 * Execute the shell for the given job.
 *
 * See Job_CatchOutput for handling the output of the shell.
 */
static void
JobExec(Job *job, char *args)
{
	STARTUPINFOA si = {sizeof si, 0};
	PROCESS_INFORMATION pi;

	if (DEBUG(JOB)) {
		debug_printf("Running %s\n", job->node->name);
		debug_printf("\tCommand: ");
		debug_printf("%s\n", args);
	}

	/*
	 * Some jobs produce no output and it's disconcerting to have
	 * no feedback of their running (since they produce no output, the
	 * banner with their name in it never appears). This is an attempt to
	 * provide that feedback, even if nothing follows it.
	 */
	if (job->echo)
		SwitchOutputTo(job->node);

	/* No interruptions until this job is on the `jobs' list */
	JobSigLock();

	/* Pre-emptively mark job running, pid still zero though */
	job->status = JOB_ST_RUNNING;

	Var_ReexportVars();

	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = si.hStdError = job->outPipe;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	
	if (CreateProcessA(shellPath, args, NULL, NULL, TRUE,
		0, NULL, NULL, &si, &pi) == 0)
		Punt("could not create process: %s", strerr(GetLastError()));
	CloseHandle(pi.hThread);

	job->handle = pi.hProcess;
	job->pid = pi.dwProcessId;

	Trace_Log(JOBSTART, job);

	/*
	 * Set the current position in the buffer to the beginning
	 * and mark another stream to watch in the outputs mask
	 */
	job->curPos = 0;

	free(args);
	if (job->cmdBuffer != NULL) {
		Buf_Done(job->cmdBuffer);

		free(job->cmdBuffer);
		job->cmdBuffer = NULL;
	}

	/* Now that the job is actually running, add it to the table. */
	if (DEBUG(JOB)) {
		debug_printf("JobExec(%s): handle %p added to jobs table\n",
		    job->node->name, job->handle);
		DumpJobs("job started");
	}
	JobSigUnlock();
}

/* Create the argv needed to execute the shell for a given job. */
static char *
JobMakeArgv(Job *job)
{
	size_t i;
	char *args;
	const char *fmt = "\"%s\" /c \"%s\"";

	job->cmdBuffer->data[job->cmdBuffer->len - 1] = '\0';
	for (i = 0; i < job->cmdBuffer->len; i++)
		if (job->cmdBuffer->data[i] == '\n')
			job->cmdBuffer->data[i] = shell->separator;

	args = bmake_malloc((size_t)snprintf(NULL, 0, fmt,
		shellPath, job->cmdBuffer->data) + 1);
	sprintf(args, fmt, shellPath, job->cmdBuffer->data);

	return args;
}

static void
JobWriteShellCommands(Job *job, GNode *gn, bool *out_run)
{
	/*
	 * tfile is the name of a file into which all shell commands
	 * are put. It is removed before the child shell is executed,
	 * unless DEBUG(SCRIPT) is set.
	 */

	job->cmdBuffer = bmake_malloc(sizeof *job->cmdBuffer);
	Buf_Init(job->cmdBuffer);

#ifdef USE_META
	if (useMeta) {
		meta_job_start(job, gn);
		if (gn->type & OP_SILENT)	/* might have changed */
			job->echo = false;
	}
#endif

	*out_run = JobWriteCommands(job);

	if (DEBUG(SCRIPT)) {
		char tfile[MAXPATHLEN];
		FILE *fp = mkTempFile(tfile, sizeof tfile);

		if (fp == NULL)
			Punt("could not open %s", tfile);

		fprintf(fp, "%s", job->cmdBuffer->data);
		fclose(fp);
	}
}

/*
 * Start a target-creation process going for the target described by gn.
 *
 * Results:
 *	JOB_ERROR if there was an error in the commands, JOB_FINISHED
 *	if there isn't actually anything left to do for the job and
 *	JOB_RUNNING if the job has been started.
 *
 * Details:
 *	A new Job node is created and added to the list of running
 *	jobs. A child shell created.
 *
 * NB: The return value is ignored by everyone.
 */
static JobStartResult
JobStart(GNode *gn, bool special)
{
	Job *job;		/* new job descriptor */
	char *args;
	bool cmdsOK;		/* true if the nodes commands were all right */
	bool run;

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == JOB_ST_FREE)
			break;
	}
	if (job >= job_table_end)
		Punt("JobStart no job slots vacant");

	memset(job, 0, sizeof *job);
	job->node = gn;
	job->tailCmds = NULL;
	job->status = JOB_ST_SET_UP;

	job->special = special || gn->type & OP_SPECIAL;
	job->ignerr = opts.ignoreErrors || gn->type & OP_IGNORE;
	job->echo = !(opts.silent || gn->type & OP_SILENT);

	/*
	 * Check the commands now so any attributes from .DEFAULT have a
	 * chance to migrate to the node.
	 */
	cmdsOK = Job_CheckCommands(gn, Error);

	if (Lst_IsEmpty(&gn->commands)) {
		run = false;

		/*
		 * We're serious here, but if the commands were bogus, we're
		 * also dead...
		 */
		if (!cmdsOK) {
			PrintOnError(gn, "\n");	/* provide some clue */
			DieHorribly();
		}
	} else if (((gn->type & OP_MAKE) && !opts.noRecursiveExecute) ||
	    (!opts.noExecute && !opts.touch)) {
		/*
		 * The above condition looks very similar to
		 * GNode_ShouldExecute but is subtly different.  It prevents
		 * that .MAKE targets are touched since these are usually
		 * virtual targets.
		 */

		/*
		 * We're serious here, but if the commands were bogus, we're
		 * also dead...
		 */
		if (!cmdsOK) {
			PrintOnError(gn, "\n");	/* provide some clue */
			DieHorribly();
		}

		JobWriteShellCommands(job, gn, &run);
	} else if (!GNode_ShouldExecute(gn)) {
		/*
		 * Just write all the commands to stdout in one fell swoop.
		 * This still sets up job->tailCmds correctly.
		 */
		SwitchOutputTo(gn);
		job->cmdBuffer = bmake_malloc(sizeof *job->cmdBuffer);
		Buf_Init(job->cmdBuffer);
		if (cmdsOK)
			JobWriteCommands(job);

		printf("%s", job->cmdBuffer->data);

		run = false;
	} else {
		Job_Touch(gn, job->echo);
		run = false;
	}

	/* If we're not supposed to execute a shell, don't. */
	if (!run) {
		if (!job->special)
			Job_TokenReturn();

		/* Close the command buffer if we opened one */
		if (job->cmdBuffer != NULL) {
			Buf_Done(job->cmdBuffer);

			free(job->cmdBuffer);
			job->cmdBuffer = NULL;
		}

		/*
		 * We only want to work our way up the graph if we aren't
		 * here because the commands for the job were no good.
		 */
		if (cmdsOK && aborting == ABORT_NONE) {
			JobSaveCommands(job);
			job->node->made = MADE;
			Make_Update(job->node);
		}
		job->status = JOB_ST_FREE;
		return cmdsOK ? JOB_FINISHED : JOB_ERROR;
	}

	/*
	 * Set up the control arguments to the shell. This is based on the
	 * flags set earlier for this job.
	 */
	args = JobMakeArgv(job);

	/* Create the pipe by which we'll get the shell's output. */
	JobCreatePipe(job);

	JobExec(job, args);
	return JOB_RUNNING;
}

/*
 * This function is called whenever there is something to read on the pipe.
 * We collect more output from the given job and store it in the job's
 * outBuf. If this makes up a line, we print it tagged by the job's
 * identifier, as necessary.
 *
 * In the output of the shell, the 'noPrint' lines are removed. If the
 * command is not alone on the line (the character after it is not \0 or
 * \n), we do print whatever follows it.
 *
 * Input:
 *	job		the job whose output needs printing
 *	finish		true if this is the last time we'll be called
 *			for this job
 */
static void
CollectOutput(Job *job, bool finish)
{
	bool gotNL;		/* true if got a newline */
	bool fbuf;		/* true if our buffer filled up */
	size_t nr;		/* number of bytes read */
	size_t i;		/* auxiliary index into outBuf */
	size_t max;		/* limit for i (end of current data) */
	DWORD nRead;		/* (Temporary) number of bytes read */

	/* Read as many bytes as will fit in the buffer. */
again:
	gotNL = false;
	fbuf = false;

	if (PeekNamedPipe(job->inPipe, NULL, 0, NULL, &nRead, NULL) == 0)
		Punt("failed to peek pipe: %s", strerr(GetLastError()));
	if (nRead == 0)
		return;
	if (ReadFile(job->inPipe, &job->outBuf[job->curPos],
		(DWORD)(JOB_BUFSIZE - job->curPos), &nRead, NULL) == 0)
		Punt("failed to read from job pipe: %s", strerr(GetLastError()));
	nr = (size_t)nRead;

	if (nr == 0)
		finish = false;	/* stop looping */

	/*
	 * If we hit the end-of-file (the job is dead), we must flush its
	 * remaining output, so pretend we read a newline if there's any
	 * output remaining in the buffer.
	 */
	if (nr == 0 && job->curPos != 0) {
		job->outBuf[job->curPos] = '\n';
		nr = 1;
	}

	max = job->curPos + nr;
	for (i = job->curPos; i < max; i++)
		if (job->outBuf[i] == '\0')
			job->outBuf[i] = ' ';

	/* Look for the last newline in the bytes we just got. */
	for (i = job->curPos + nr - 1;
	     i >= job->curPos && i != (size_t)-1; i--) {
		if (job->outBuf[i] == '\n') {
			gotNL = true;
			break;
		}
	}

	if (!gotNL) {
		job->curPos += nr;
		if (job->curPos == JOB_BUFSIZE) {
			/*
			 * If we've run out of buffer space, we have no choice
			 * but to print the stuff. sigh.
			 */
			fbuf = true;
			i = job->curPos;
		}
	}
	if (gotNL || fbuf) {
		/*
		 * Need to send the output to the screen. Null terminate it
		 * first, overwriting the newline character if there was one.
		 * So long as the line isn't one we should filter (according
		 * to the shell description), we print the line, preceded
		 * by a target banner if this target isn't the same as the
		 * one for which we last printed something.
		 * The rest of the data in the buffer are then shifted down
		 * to the start of the buffer and curPos is set accordingly.
		 */
		job->outBuf[i] = '\0';
		if (i >= job->curPos) {
			char *cp;

			/*
			 * FIXME: SwitchOutputTo should be here, according to
			 * the comment above.  But since PrintOutput does not
			 * do anything in the default shell, this bug has gone
			 * unnoticed until now.
			 */
			cp = job->outBuf;

			/*
			 * There's still more in the output buffer. This time,
			 * though, we know there's no newline at the end, so
			 * we add one of our own free will.
			 */
			if (*cp != '\0') {
				if (!opts.silent)
					SwitchOutputTo(job->node);
#ifdef USE_META
				if (useMeta) {
					meta_job_output(job, cp,
					    gotNL ? "\n" : "");
				}
#endif
				(void)fprintf(stdout, "%s%s", cp,
				    gotNL ? "\n" : "");
				(void)fflush(stdout);
			}
		}
		/*
		 * max is the last offset still in the buffer. Move any
		 * remaining characters to the start of the buffer and
		 * update the end marker curPos.
		 */
		if (i < max) {
			(void)memmove(job->outBuf, &job->outBuf[i + 1],
			    max - (i + 1));
			job->curPos = max - (i + 1);
		} else {
			assert(i == max);
			job->curPos = 0;
		}
	}
	if (finish) {
		/*
		 * If the finish flag is true, we must loop until we hit
		 * end-of-file on the pipe. This is guaranteed to happen
		 * eventually since the other end of the pipe is now closed
		 * (we closed it explicitly and the child has exited). When
		 * we do get an EOF, finish will be set false and we'll fall
		 * through and out.
		 */
		goto again;
	}
}

static void
JobRun(GNode *targ)
{
#if 0
	/*
	 * Unfortunately it is too complicated to run .BEGIN, .END, and
	 * .INTERRUPT job in the parallel job module.  As of 2020-09-25,
	 * unit-tests/deptgt-end-jobs.mk hangs in an endless loop.
	 *
	 * Running these jobs in compat mode also guarantees that these
	 * jobs do not overlap with other unrelated jobs.
	 */
	GNodeList lst = LST_INIT;
	Lst_Append(&lst, targ);
	(void)Make_Run(&lst);
	Lst_Done(&lst);
	JobStart(targ, true);
	while (jobTokensRunning != 0) {
		Job_CatchOutput();
	}
#else
	Compat_Make(targ, targ);
	/* XXX: Replace with GNode_IsError(gn) */
	if (targ->made == ERROR) {
		PrintOnError(targ, "\n\nStop.\n");
		exit(1);
	}
#endif
}

/*
 * Handle the exit of a child. Called from Make_Make.
 *
 * The job descriptor is removed from the list of children.
 *
 * Notes:
 *	We do waits, blocking or not, according to the wisdom of our
 *	caller, until there are no more children to report. For each
 *	job, call JobFinish to finish things off.
 */
void
Job_CatchChildren(void)
{
	Job *job;

	/* Don't even bother if we know there's no one around. */
	if (jobTokensRunning == 0)
		return;

	for (job = job_table; job < job_table_end; job++) {
		DWORD status; /* Exit/termination status */

		if (job->status < JOB_ST_RUNNING)
			continue;
		if (WaitForSingleObject(job->handle, INFINITE) == WAIT_FAILED)
			Punt("failed to wait for process: %s",
				strerr(GetLastError()));
		if (GetExitCodeProcess(job->handle, &status) == 0)
			Punt("failed to get exit code for process: %s",
				strerr(GetLastError()));

		job->status = JOB_ST_FINISHED;
		job->exit_status = status;

		JobFinish(job, status);
	}
}

/*
 * Start the creation of a target. Basically a front-end for JobStart used by
 * the Make module.
 */
void
Job_Make(GNode *gn)
{
	(void)JobStart(gn, false);
}

static void
InitShellNameAndPath(void)
{
	shellName = shell->name;

#ifdef DEFSHELL_CUSTOM
	if (isAbs(shellName[0])) {
		shellPath = shellName;
		shellName = str_basename(shellPath);
		return;
	}
#endif
#ifdef DEFSHELL_PATH
	shellPath = DEFSHELL_PATH;
#else
	if ((shellPath = getenv("COMSPEC")) == NULL ||
		shellPath[0] == '\0' ||
		strcmp(shellName, str_basename(shellPath)) != 0)
		Punt("could not find %s's path via ComSpec", shellName);
#endif
}

void
Shell_Init(void)
{
	if (shellPath == NULL)
		InitShellNameAndPath();

	Var_SetWithFlags(SCOPE_CMDLINE, ".SHELL", shellPath, VAR_SET_READONLY);
}

void
Job_SetPrefix(void)
{
	if (targPrefix != NULL) {
		free(targPrefix);
	} else if (!Var_Exists(SCOPE_GLOBAL, ".MAKE.JOB.PREFIX")) {
		Global_Set(".MAKE.JOB.PREFIX", "---");
	}

	targPrefix = Var_Subst("${.MAKE.JOB.PREFIX}",
	    SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */
}

static void
AddSig(int sig, SignalProc handler)
{
	int i = 0;

	if (signal(sig, SIG_IGN) == SIG_IGN)
		return;

	for (; caught_signals[i] != NSIG; i++);
	caught_signals[i] = sig;
	signal_handlers[i] = handler;
	(void)signal(sig, handler);
}

/* Initialize the process module. */
void
Job_Init(void)
{
	Job_SetPrefix();
	/* Allocate space for all the job info */
	job_table = bmake_malloc((size_t)opts.maxJobs * sizeof *job_table);
	memset(job_table, 0, (size_t)opts.maxJobs * sizeof *job_table);
	job_table_end = job_table + opts.maxJobs;

	aborting = ABORT_NONE;
	job_errors = 0;

	/*
	 * There is a non-zero chance that we already have children.
	 * eg after 'make -f- <<EOF'
	 * Since their termination causes a 'Child (pid) not in table'
	 * message, Collect the status of any that are already dead, and
	 * suppress the error message if there are any undead ones.
	 */

	Shell_Init();

	/* These are permanent entries and take slots 0 and 1 */

	for (int i = 0; i < SIGMAX; i++)
		caught_signals[i] = NSIG;

	/*
  	 * Catch SIGINT and SIGTERM if they aren't ignored.
	 * JobPassSig will take care of calling JobInterrupt if appropriate.
	 */
	AddSig(SIGINT, JobPassSig_int);
	AddSig(SIGTERM, JobPassSig_term);

	(void)Job_RunTarget(".BEGIN", NULL);
	/*
	 * Create the .END node now, even though no code in the unit tests
	 * depends on it.  See also Targ_GetEndNode in Compat_MakeAll.
	 */
	(void)Targ_GetEndNode();
}

static void
DelSig(int sig)
{
	for (int i = 0; i < SIGMAX; i++)
		if (caught_signals[i] == sig) {
			caught_signals[i] = NSIG;
			return;
		}
}

static void
JobSigReset(void)
{
	DelSig(SIGINT);
	DelSig(SIGTERM);
}

/* Find a shell in 'shells' given its name, or return NULL. */
static Shell *
FindShellByName(const char *name)
{
	Shell *sh = shells;
	const Shell *shellsEnd = sh + sizeof shells / sizeof shells[0];

	for (sh = shells; sh < shellsEnd; sh++) {
		if (strcmp(name, sh->name) == 0)
			return sh;
	}
	return NULL;
}

/*
 * Parse a shell specification and set up 'shell', shellPath and
 * shellName appropriately.
 *
 * Input:
 *	line		The shell spec
 *
 * Results:
 *	false if the specification was incorrect.
 *
 * Side Effects:
 *	'shell' points to a Shell structure (either predefined or
 *	created from the shell spec), shellPath is the full path of the
 *	shell described by 'shell', while shellName is just the
 *	final component of shellPath.
 *
 * Notes:
 *	A shell specification consists of a .SHELL target, with dependency
 *	operator, followed by a series of blank-separated words. Double
 *	quotes can be used to use blanks in words. A backslash escapes
 *	anything (most notably a double-quote and a space) and
 *	provides the functionality it does in C. Each word consists of
 *	keyword and value separated by an equal sign. There should be no
 *	unnecessary spaces in the word. The keywords are as follows:
 *	    name	Name of shell.
 *	    path	Location of shell.
 *	    check	Template of command to echo a command
 *	    ignore	Template of command to run a command,
 *				dont exit on error
 *		errout	Template of command to run a command,
 *				exit on error
 *		comment The character used for comments by the shell.
 *					Can be '\0' if no such character exists.
 *		separator The character used to execute multiple commands
 *					on a single line, e.g '&' for CMD.
 */
bool
Job_ParseShell(char *line)
{
	Words wordsList;
	char **words;
	char **argv;
	size_t argc;
	char *path;
	Shell newShell;
	bool fullSpec = false;
	Shell *sh;

	/* XXX: don't use line as an iterator variable */
	pp_skip_whitespace(&line);

	free(shell_freeIt);

	memset(&newShell, 0, sizeof newShell);

	/*
	 * Str_Words treats backslashes as escape sequences.
	 * Replace all instances of '\' with '/'.
	 */
	if ((path = strstr(line, "path=")) != NULL) {
		bool quoted = false;

		path += 5;
		if (*path == '"') {
			quoted = true;
			path++;
		}

		for (; *path != '\0' && *path != (quoted ? '"' : ' ');
			path++)
			if (*path == '\\')
				*path = '/';

		if (quoted && *path != '"') {
			Error("Unterminated quoted string [%s]", line);
			return false;
		}
	}
	
	/*
	 * Parse the specification by keyword
	 */
	wordsList = Str_Words(line, true);
	words = wordsList.words;
	argc = wordsList.len;
	path = wordsList.freeIt;
	if (words == NULL) {
		Error("Unterminated quoted string [%s]", line);
		return false;
	}
	shell_freeIt = path;

	for (path = NULL, argv = words; argc != 0; argc--, argv++) {
		char *arg = *argv;
		if (strncmp(arg, "path=", 5) == 0) {
			path = arg + 5;
		} else if (strncmp(arg, "name=", 5) == 0) {
			newShell.name = arg + 5;
		} else {
			if (strncmp(arg, "check=", 6) == 0) {
				newShell.echoTmpl = arg + 6;
			} else if (strncmp(arg, "ignore=", 7) == 0) {
				newShell.runIgnTmpl = arg + 7;
			} else if (strncmp(arg, "errout=", 7) == 0) {
				newShell.runChkTmpl = arg + 7;
			} else if (strncmp(arg, "comment=", 8) == 0) {
				newShell.commentChar = arg[8];
			} else if (strncmp(arg, "separator=", 10) == 0) {
				newShell.separator = arg[10];
			} else {
				Parse_Error(PARSE_FATAL,
				    "Unknown keyword \"%s\"", arg);
				free(words);
				return false;
			}
			fullSpec = true;
		}
	}

	if (path == NULL) {
		/*
		 * If no path was given, the user wants one of the
		 * pre-defined shells, yes? So we find the one s/he wants
		 * with the help of FindShellByName and set things up the
		 * right way. shellPath will be set up by Shell_Init.
		 */
		if (newShell.name == NULL) {
			Parse_Error(PARSE_FATAL,
			    "Neither path nor name specified");
			free(words);
			return false;
		} else {
			if ((sh = FindShellByName(newShell.name)) == NULL) {
				Parse_Error(PARSE_WARNING,
				    "%s: No matching shell", newShell.name);
				free(words);
				return false;
			}
			shell = sh;
			shellName = newShell.name;
			if (shellPath != NULL) {
				/*
				 * Shell_Init has already been called!
				 * Do it again.
				 */
				free(UNCONST(shellPath));
				shellPath = NULL;
				Shell_Init();
			}
		}
	} else {
		/*
		 * The user provided a path. If s/he gave nothing else
		 * (fullSpec is false), try and find a matching shell in the
		 * ones we know of. Else we just take the specification at
		 * its word and copy it to a new location. In either case,
		 * we need to record the path the user gave for the shell.
		 */
		shellPath = path;
		path = lastSlash(path);
		if (path == NULL) {
			path = UNCONST(shellPath);
		} else {
			/*
			 * For some reason, cmd.exe insists that that last slash
			 * is a backslash.
			 */
			*path = '\\';
			path++;
		}
		if (newShell.name != NULL) {
			shellName = newShell.name;
		} else {
			shellName = path;
		}
		if (!fullSpec) {
			if ((sh = FindShellByName(shellName)) == NULL) {
				Parse_Error(PARSE_WARNING,
				    "%s: No matching shell", shellName);
				free(words);
				return false;
			}
			shell = sh;
		} else {
			shell = bmake_malloc(sizeof *shell);
			*shell = newShell;
		}
		/* this will take care of shellErrFlag */
		Shell_Init();
	}

	if (shell->echoTmpl == NULL)
		shell->echoTmpl = "";
	if (shell->runIgnTmpl == NULL)
		shell->runIgnTmpl = "%s\n";

	/*
	 * Do not free up the words themselves, since they might be in use
	 * by the shell specification.
	 */
	free(words);
	return true;
}

/*
 * Handle the receipt of an interrupt.
 *
 * All children are killed. Another job will be started if the .INTERRUPT
 * target is defined.
 *
 * Input:
 *	runINTERRUPT	Non-zero if commands for the .INTERRUPT target
 *			should be executed
 *	signo		signal received
 */
static void
JobInterrupt(bool runINTERRUPT, int signo)
{
	Job *job;		/* job descriptor in that element */
	GNode *interrupt;	/* the node describing the .INTERRUPT target */
	GNode *gn;

	aborting = ABORT_INTERRUPT;

	JobSigLock();

	for (job = job_table; job < job_table_end; job++) {
		if (job->status != JOB_ST_RUNNING)
			continue;

		gn = job->node;

		JobDeleteTarget(gn);
		if (job->handle != NULL) {
			DEBUG1(JOB,
				"JobInterrupt terminating child %p.\n",
			    job->handle);
			TerminateProcess(job->handle, signo);
		}
	}

	JobSigUnlock();

	if (runINTERRUPT && !opts.touch) {
		interrupt = Targ_FindNode(".INTERRUPT");
		if (interrupt != NULL) {
			opts.ignoreErrors = false;
			JobRun(interrupt);
		}
	}
	Trace_Log(MAKEINTR, NULL);
	exit(signo);		/* XXX: why signo? */
}

/*
 * Do the final processing, i.e. run the commands attached to the .END target.
 *
 * Return the number of errors reported.
 */
int
Job_Finish(void)
{
	GNode *endNode = Targ_GetEndNode();
	if (!Lst_IsEmpty(&endNode->commands) ||
	    !Lst_IsEmpty(&endNode->children)) {
		if (job_errors != 0) {
			Error("Errors reported so .END ignored");
		} else {
			JobRun(endNode);
		}
	}
	return job_errors;
}

/* Clean up any memory used by the jobs module. */
void
Job_End(void)
{
#ifdef CLEANUP
	free(shell_freeIt);
#endif
}

/*
 * Waits for all running jobs to finish and returns.
 * Sets 'aborting' to ABORT_WAIT to prevent other jobs from starting.
 */
void
Job_Wait(void)
{
	aborting = ABORT_WAIT;
	while (jobTokensRunning != 0) {
		Job_CatchChildren();
	}
	aborting = ABORT_NONE;
}

/*
 * Abort all currently running jobs without handling output or anything.
 * This function is to be called only in the event of a major error.
 * Most definitely NOT to be called from JobInterrupt.
 *
 * All children are killed, not just the firstborn.
 */
void
Job_AbortAll(void)
{
	Job *job;		/* the job descriptor in that element */

	aborting = ABORT_ERROR;

	if (jobTokensRunning != 0) {
		for (job = job_table; job < job_table_end; job++) {
			if (job->status != JOB_ST_RUNNING)
				continue;
			/*
			 * kill the child process
			 */
			TerminateProcess(job->handle, 0);
		}
	}
}

/*
 * Put a token (back) into the job pipe.
 * This allows a make process to start a build job.
 */
static void
JobTokenAdd(void)
{
	DWORD ret;
	char tok = JOB_TOKENS[aborting], tok1;

	/* If we are depositing an error token flush everything else */
	if (tok != '+') {
		while (ReadFile(tokenWaitJob.inPipe, &tok1, 1, NULL, NULL) != 0);

		if ((ret = GetLastError()) != ERROR_NO_DATA)
			Fatal("Failed to read from pipe: %s", strerr(ret));
	}

	DEBUG3(JOB, "(%lu) aborting %d, deposit token %c\n",
	    myPid, aborting, tok);

	if (WriteFile(tokenWaitJob.outPipe, &tok, 1, NULL, NULL) == 0)
		Fatal("Failed to write to pipe: %s", strerr(GetLastError()));
}

/* Prep the job token pipe in the root make process. */
void
Job_ServerStart(int max_tokens, HANDLE jp_0, HANDLE jp_1)
{
	int i;
	char jobarg[64];

	if (jp_0 != NULL && jp_1 != NULL) {
		/* Pipe passed in from parent */
		tokenWaitJob.inPipe = jp_0;
		tokenWaitJob.outPipe = jp_1;
		return;
	}

	JobCreatePipe(&tokenWaitJob);

	/* make the read pipe inheritable */
	if (SetHandleInformation(tokenWaitJob.inPipe, HANDLE_FLAG_INHERIT,
		HANDLE_FLAG_INHERIT) == 0)
		Punt("failed to set pipe attributes: %s", strerr(GetLastError()));

	snprintf(jobarg, sizeof jobarg, "%p,%p",
	    tokenWaitJob.inPipe, tokenWaitJob.outPipe);

	Global_Append(MAKEFLAGS, "-J");
	Global_Append(MAKEFLAGS, jobarg);

	/*
	 * Preload the job pipe with one token per job, save the one
	 * "extra" token for the primary job.
	 *
	 * XXX should clip maxJobs against PIPE_BUF -- if max_tokens is
	 * larger than the write buffer size of the pipe, we will
	 * deadlock here.
	 */
	for (i = 1; i < max_tokens; i++)
		JobTokenAdd();
}

/* Return a withdrawn token to the pool. */
void
Job_TokenReturn(void)
{
	jobTokensRunning--;
	if (jobTokensRunning < 0)
		Punt("token botch");
	if (jobTokensRunning != 0 || JOB_TOKENS[aborting] != '+')
		JobTokenAdd();
}

/*
 * Attempt to withdraw a token from the pool.
 *
 * Returns true if a token was withdrawn, and false if the pool is currently
 * empty.
 */
bool
Job_TokenWithdraw(void)
{
	char tok, tok1;
	DWORD count, ret = 0;

	DEBUG3(JOB, "Job_TokenWithdraw(%lu): aborting %d, running %d\n",
	    myPid, aborting, jobTokensRunning);

	if (aborting != ABORT_NONE || (jobTokensRunning >= opts.maxJobs))
		return false;

	if (ReadFile(tokenWaitJob.inPipe, &tok, 1, &count, NULL) == 0 &&
		(ret = GetLastError()) != ERROR_NO_DATA)
		Fatal("Failed to read from pipe: %s", strerr(ret));

	if (ret == ERROR_NO_DATA && jobTokensRunning != 0) {
		DEBUG1(JOB, "(%lu) blocked for token\n", myPid);
		return false;
	}

	if (count == 1 && tok != '+') {
		/* make being aborted - remove any other job tokens */
		DEBUG2(JOB, "(%lu) aborted by token %c\n", myPid, tok);

		while (ReadFile(tokenWaitJob.inPipe, &tok1, 1, &count, NULL) != 0);
		if ((ret = GetLastError()) != ERROR_NO_DATA)
			Fatal("Failed to read from pipe: %s", strerr(ret));

		/* And put the stopper back */
		if (WriteFile(tokenWaitJob.outPipe, &tok, 1, NULL, NULL) == 0)
			Fatal("Failed to write to pipe: %s", strerr(GetLastError()));
		if (shouldDieQuietly(NULL, 1))
			exit(6);	/* we aborted */
		Fatal("A failure has been detected "
		      "in another branch of the parallel make");
	}

	if (count == 1 && jobTokensRunning == 0)
		/* We didn't want the token really */
		if (WriteFile(tokenWaitJob.outPipe, &tok, 1, NULL, NULL) == 0)
			Fatal("Failed to write to pipe: %s", strerr(GetLastError()));

	jobTokensRunning++;
	DEBUG1(JOB, "(%lu) withdrew token\n", myPid);
	return true;
}

/*
 * Run the named target if found. If a filename is specified, then set that
 * to the sources.
 *
 * Exits if the target fails.
 */
bool
Job_RunTarget(const char *target, const char *fname)
{
	GNode *gn = Targ_FindNode(target);
	if (gn == NULL)
		return false;

	if (fname != NULL)
		Var_Set(gn, ALLSRC, fname);

	JobRun(gn);
	/* XXX: Replace with GNode_IsError(gn) */
	if (gn->made == ERROR) {
		PrintOnError(gn, "\n\nStop.\n");
		exit(1);
	}
	return true;
}
