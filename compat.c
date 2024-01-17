/*	$NetBSD: compat.c,v 1.252 2024/01/05 23:22:06 rillig Exp $	*/

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
 * This file implements the full-compatibility mode of make, which makes the
 * targets without parallelism and without a custom shell.
 *
 * Interface:
 *	Compat_MakeAll	Initialize this module and make the given targets.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <malloc.h>

#include "make.h"
#include "dir.h"
#include "job.h"

/*	"@(#)compat.c	8.2 (Berkeley) 3/19/94"	*/

static GNode *curTarg = NULL;
static HANDLE compatChild = NULL;
static bool signaled = false;

/*
 * Delete the file of a failed, interrupted, or otherwise duffed target,
 * unless inhibited by .PRECIOUS.
 */
static void
CompatDeleteTarget(GNode *gn)
{
	if (gn != NULL && !GNode_IsPrecious(gn) &&
	    (gn->type & OP_PHONY) == 0) {
		const char *file = GNode_VarTarget(gn);
		if (!opts.noExecute && unlink_file(file) == 0)
			Error("*** %s removed", file);
	}
}

/*
 * Interrupt the creation of the current target and remove it if it ain't
 * precious. Then exit.
 *
 * If .INTERRUPT exists, its commands are run first WITH INTERRUPTS IGNORED.
 *
 * XXX: is .PRECIOUS supposed to inhibit .INTERRUPT? I doubt it, but I've
 * left the logic alone for now. - dholland 20160826
 */
static void
CompatInterrupt_int(void)
{
	/*
	 * if Compat_Make is run, compatChild
	 * loses its value
	 */
	HANDLE savedChild = compatChild;

	CompatDeleteTarget(curTarg);

	if (curTarg != NULL && !GNode_IsPrecious(curTarg)) {
		/*
		 * Run .INTERRUPT
		 */
		GNode *gn = Targ_FindNode(".INTERRUPT");
		if (gn != NULL) {
			Compat_Make(gn, gn);
		}
	}

	signaled = true;
	if (savedChild != NULL)
		(void)TerminateProcess(savedChild, 0);
	else
		exit(2);
}

static void
CompatInterrupt_term(void)
{
	CompatDeleteTarget(curTarg);

	signaled = true;
	if (compatChild != NULL)
		(void)TerminateProcess(compatChild, 0);
	else
		exit(2);
}

static void
DebugFailedTarget(const char *cmd, const GNode *gn)
{
	const char *p = cmd;
	debug_printf("\n*** Failed target:  %s\n*** Failed command: ",
	    gn->name);

	/*
	 * Replace runs of whitespace with a single space, to reduce the
	 * amount of whitespace for multi-line command lines.
	 */
	while (*p != '\0') {
		if (ch_isspace(*p)) {
			debug_printf(" ");
			cpp_skip_whitespace(&p);
		} else {
			debug_printf("%c", *p);
			p++;
		}
	}
	debug_printf("\n");
}

/*
 * Execute the next command for a target. If the command returns an error,
 * the node's made field is set to ERROR and creation stops.
 *
 * Input:
 *	cmdp		Command to execute
 *	gn		Node from which the command came
 *	ln		List node that contains the command
 *
 * Results:
 *	true if the command succeeded.
 */
bool
Compat_RunCommand(const char *cmdp, GNode *gn, StringListNode *ln)
{
	char *cmdStart;		/* Start of expanded command */
	bool silent;		/* Don't print command */
	bool doIt;		/* Execute even if -n */
	bool errCheck;	/* Check errors */
	DWORD status;		/* Child's exit code */
	PROCESS_INFORMATION pi;	/* Process information for the shell we start */

	const char *cmd = cmdp;

	silent = (gn->type & OP_SILENT) != OP_NONE;
	errCheck = !(gn->type & OP_IGNORE);
	doIt = false;

	cmdStart = Var_Subst(cmd, gn, VARE_WANTRES);
	/* TODO: handle errors */

	if (cmdStart[0] == '\0') {
		free(cmdStart);
		return true;
	}
	cmd = cmdStart;
	LstNode_Set(ln, cmdStart);

	if (gn->type & OP_SAVE_CMDS) {
		GNode *endNode = Targ_GetEndNode();
		if (gn != endNode) {
			/*
			 * Append the expanded command, to prevent the
			 * local variables from being interpreted in the
			 * scope of the .END node.
			 *
			 * A probably unintended side effect of this is that
			 * the expanded command will be expanded again in the
			 * .END node.  Therefore, a literal '$' in these
			 * commands must be written as '$$$$' instead of the
			 * usual '$$'.
			 */
			Lst_Append(&endNode->commands, cmdStart);
			return true;
		}
	}
	if (strcmp(cmdStart, "...") == 0) {
		gn->type |= OP_SAVE_CMDS;
		return true;
	}

	for (;;) {
		if (*cmd == '@')
			silent = !DEBUG(LOUD);
		else if (*cmd == '-')
			errCheck = false;
		else if (*cmd == '+')
			doIt = true;
		else if (!ch_isspace(*cmd))
			/* Ignore whitespace for compatibility with gnu make */
			break;
		cmd++;
	}

	while (ch_isspace(*cmd))
		cmd++;

	if (cmd[0] == '\0')
		return true;

	if (!silent || !GNode_ShouldExecute(gn)) {
		printf("%s\n", cmd);
		fflush(stdout);
	}

	if (!doIt && !GNode_ShouldExecute(gn))
		return true;

	DEBUG1(JOB, "Execute: '%s'\n", cmd);

	if (shellPath == NULL)
		Shell_Init();		/* we need shellPath */

	{
		const char *args = Shell_GetArgs();
		char *tmp = _alloca((size_t)snprintf(NULL, 0,
			cmdFmt, shellPath, args, cmd) + 1);

		sprintf(tmp, cmdFmt, shellPath, args, cmd);
		
		cmd = tmp;
	}

#ifdef USE_META
	if (useMeta)
		meta_compat_start();
#endif

	Var_ReexportVars();

#ifdef USE_META
	if (useMeta) {
		STARTUPINFOA si = {sizeof si, 0};
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = si.hStdError = meta_compat_pipe()[1];
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		if (CreateProcessA(shellPath, cmd, NULL, NULL, TRUE, 0, NULL,
			NULL, &si, &pi) == 0)
			Punt("could not create process: %s", strerr(GetLastError()));
	}
	else
#endif
	if (CreateProcessA(shellPath, cmd, NULL, NULL, FALSE, 0, NULL,
		NULL, &(STARTUPINFOA){sizeof (STARTUPINFOA), 0}, &pi) == 0)
		Punt("could not create process: %s", strerr(GetLastError()));

	compatChild = pi.hProcess;

	/* XXX: Memory management looks suspicious here. */
	/* XXX: Setting a list item to NULL is unexpected. */
	LstNode_SetNull(ln);

	/* The child is off and running. Now all we can do is wait... */
#ifdef USE_META
	if (useMeta) {
		while ((status = WaitForSingleObject(pi.hProcess, PROCESSWAIT))
			== WAIT_TIMEOUT) {
			DWORD avail;

			if (PeekNamedPipe(meta_compat_pipe()[0], NULL, 0, NULL, &avail, NULL)
				== 0)
				Punt("failed to peek pipe: %s", strerr(GetLastError()));

			if (avail >= PIPESZ)
				meta_compat_catch(cmdStart);
		}

		if (status == WAIT_FAILED)
			Punt("failed to wait for process: %s", strerr(GetLastError()));
	} else
#endif
	if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
		Punt("failed to wait for process: %s", strerr(GetLastError()));

	(void)GetExitCodeProcess(pi.hProcess, &status);

#ifdef USE_META
	if (useMeta) {
		meta_compat_catch(cmdStart);
		meta_compat_done();
	}
#endif

	if (status != 0) {
		if (DEBUG(ERROR))
			DebugFailedTarget(cmdp, gn);
		printf("*** Error code %ld", status);

		if (errCheck) {
#ifdef USE_META
			if (useMeta)
				meta_job_error(NULL, gn, false, status);
#endif
			gn->made = ERROR;
			if (opts.keepgoing) {
				/*
				 * Abort the current target,
				 * but let others continue.
				 */
				printf(" (continuing)\n");
			} else {
				printf("\n");
			}
			if (deleteOnError)
				CompatDeleteTarget(gn);
		} else {
			/*
			 * Continue executing commands for this target.
			 * If we return 0, this will happen...
			 */
			printf(" (ignored)\n");
			status = 0;
		}
		fflush(stdout);
	}

	free(cmdStart);
	compatChild = NULL;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	if (signaled)
		exit(2);

	return status == 0;
}

static void
RunCommands(GNode *gn)
{
	StringListNode *ln;

	for (ln = gn->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;
		if (!Compat_RunCommand(cmd, gn, ln))
			break;
	}
}

static void
MakeInRandomOrder(GNode **gnodes, GNode **end, GNode *pgn)
{
	GNode **it;
	size_t r;

	for (r = (size_t)(end - gnodes); r >= 2; r--) {
		/* Biased, but irrelevant in practice. */
		size_t i = (size_t)random() % r;
		GNode *t = gnodes[r - 1];
		gnodes[r - 1] = gnodes[i];
		gnodes[i] = t;
	}

	for (it = gnodes; it != end; it++)
		Compat_Make(*it, pgn);
}

static void
MakeWaitGroupsInRandomOrder(GNodeList *gnodes, GNode *pgn)
{
	Vector vec;
	GNodeListNode *ln;
	GNode **nodes;
	size_t i, n, start;

	Vector_Init(&vec, sizeof(GNode *));
	for (ln = gnodes->first; ln != NULL; ln = ln->next)
		*(GNode **)Vector_Push(&vec) = ln->datum;
	nodes = vec.items;
	n = vec.len;

	start = 0;
	for (i = 0; i < n; i++) {
		if (nodes[i]->type & OP_WAIT) {
			MakeInRandomOrder(nodes + start, nodes + i, pgn);
			Compat_Make(nodes[i], pgn);
			start = i + 1;
		}
	}
	MakeInRandomOrder(nodes + start, nodes + i, pgn);

	Vector_Done(&vec);
}

static void
MakeNodes(GNodeList *gnodes, GNode *pgn)
{
	GNodeListNode *ln;

	if (Lst_IsEmpty(gnodes))
		return;
	if (opts.randomizeTargets) {
		MakeWaitGroupsInRandomOrder(gnodes, pgn);
		return;
	}

	for (ln = gnodes->first; ln != NULL; ln = ln->next) {
		GNode *cgn = ln->datum;
		Compat_Make(cgn, pgn);
	}
}

static bool
MakeUnmade(GNode *gn, GNode *pgn)
{

	assert(gn->made == UNMADE);

	/*
	 * First mark ourselves to be made, then apply whatever transformations
	 * the suffix module thinks are necessary. Once that's done, we can
	 * descend and make all our children. If any of them has an error
	 * but the -k flag was given, our 'make' field will be set to false
	 * again. This is our signal to not attempt to do anything but abort
	 * our parent as well.
	 */
	gn->flags.remake = true;
	gn->made = BEINGMADE;

	if (!(gn->type & OP_MADE))
		Suff_FindDeps(gn);

	MakeNodes(&gn->children, gn);

	if (!gn->flags.remake) {
		gn->made = ABORTED;
		pgn->flags.remake = false;
		return false;
	}

	if (Lst_FindDatum(&gn->implicitParents, pgn) != NULL)
		Var_Set(pgn, IMPSRC, GNode_VarTarget(gn));

	/*
	 * All the children were made ok. Now youngestChild->mtime contains the
	 * modification time of the newest child, we need to find out if we
	 * exist and when we were modified last. The criteria for datedness
	 * are defined by GNode_IsOODate.
	 */
	DEBUG1(MAKE, "Examining %s...", gn->name);
	if (!GNode_IsOODate(gn)) {
		gn->made = UPTODATE;
		DEBUG0(MAKE, "up-to-date.\n");
		return false;
	}

	/*
	 * If the user is just seeing if something is out-of-date, exit now
	 * to tell him/her "yes".
	 */
	DEBUG0(MAKE, "out-of-date.\n");
	if (opts.query && gn != Targ_GetEndNode())
		exit(1);

	/*
	 * We need to be re-made.
	 * Ensure that $? (.OODATE) and $> (.ALLSRC) are both set.
	 */
	GNode_SetLocalVars(gn);

	/*
	 * Alter our type to tell if errors should be ignored or things
	 * should not be printed so Compat_RunCommand knows what to do.
	 */
	if (opts.ignoreErrors)
		gn->type |= OP_IGNORE;
	if (opts.silent)
		gn->type |= OP_SILENT;

	if (Job_CheckCommands(gn, Fatal)) {
		if (!opts.touch || (gn->type & OP_MAKE)) {
			curTarg = gn;
#ifdef USE_META
			if (useMeta && GNode_ShouldExecute(gn))
				meta_job_start(NULL, gn);
#endif
			RunCommands(gn);
			curTarg = NULL;
		} else {
			Job_Touch(gn, (gn->type & OP_SILENT) != OP_NONE);
		}
	} else {
		gn->made = ERROR;
	}
#ifdef USE_META
	if (useMeta && GNode_ShouldExecute(gn)) {
		if (meta_job_finish(NULL) != 0)
			gn->made = ERROR;
	}
#endif

	if (gn->made != ERROR) {
		/*
		 * If the node was made successfully, mark it so, update
		 * its modification time and timestamp all its parents.
		 * This is to keep its state from affecting that of its parent.
		 */
		gn->made = MADE;
		if (Make_Recheck(gn) == 0)
			pgn->flags.force = true;
		if (!(gn->type & OP_EXEC)) {
			pgn->flags.childMade = true;
			GNode_UpdateYoungestChild(pgn, gn);
		}
	} else if (opts.keepgoing) {
		pgn->flags.remake = false;
	} else {
		PrintOnError(gn, "\nStop.\n");
		exit(1);
	}
	return true;
}

static void
MakeOther(GNode *gn, GNode *pgn)
{

	if (Lst_FindDatum(&gn->implicitParents, pgn) != NULL) {
		const char *target = GNode_VarTarget(gn);
		Var_Set(pgn, IMPSRC, target != NULL ? target : "");
	}

	switch (gn->made) {
	case BEINGMADE:
		Error("Graph cycles through %s", gn->name);
		gn->made = ERROR;
		pgn->flags.remake = false;
		break;
	case MADE:
		if (!(gn->type & OP_EXEC)) {
			pgn->flags.childMade = true;
			GNode_UpdateYoungestChild(pgn, gn);
		}
		break;
	case UPTODATE:
		if (!(gn->type & OP_EXEC))
			GNode_UpdateYoungestChild(pgn, gn);
		break;
	default:
		break;
	}
}

/*
 * Make a target.
 *
 * If an error is detected and not being ignored, the process exits.
 *
 * Input:
 *	gn		The node to make
 *	pgn		Parent to abort if necessary
 *
 * Output:
 *	gn->made
 *		UPTODATE	gn was already up-to-date.
 *		MADE		gn was recreated successfully.
 *		ERROR		An error occurred while gn was being created,
 *				either due to missing commands or in -k mode.
 *		ABORTED		gn was not remade because one of its
 *				dependencies could not be made due to errors.
 */
void
Compat_Make(GNode *gn, GNode *pgn)
{
	if (shellName == NULL)	/* we came here from jobs */
		Shell_Init();

	if (gn->made == UNMADE && (gn == pgn || !(pgn->type & OP_MADE))) {
		if (!MakeUnmade(gn, pgn))
			goto cohorts;

		/* XXX: Replace with GNode_IsError(gn) */
	} else if (gn->made == ERROR) {
		/*
		 * Already had an error when making this.
		 * Tell the parent to abort.
		 */
		pgn->flags.remake = false;
	} else {
		MakeOther(gn, pgn);
	}

cohorts:
	MakeNodes(&gn->cohorts, pgn);
}

static void
MakeBeginNode(void)
{
	GNode *gn = Targ_FindNode(".BEGIN");
	if (gn == NULL)
		return;

	Compat_Make(gn, gn);
	if (GNode_IsError(gn)) {
		PrintOnError(gn, "\nStop.\n");
		exit(1);
	}
}

void
Compat_MakeAll(GNodeList *targs)
{
	GNode *errorNode = NULL;

	if (shellName == NULL)
		Shell_Init();

	Msg_Init(CompatInterrupt_int, CompatInterrupt_term);

	/*
	 * Create the .END node now, to keep the (debug) output of the
	 * counter.mk test the same as before 2020-09-23.  This
	 * implementation detail probably doesn't matter though.
	 */
	(void)Targ_GetEndNode();

	if (!opts.query)
		MakeBeginNode();

	/*
	 * Expand .USE nodes right now, because they can modify the structure
	 * of the tree.
	 */
	Make_ExpandUse(targs);

	while (!Lst_IsEmpty(targs)) {
		GNode *gn = Lst_Dequeue(targs);
		Compat_Make(gn, gn);

		if (gn->made == UPTODATE) {
			printf("`%s' is up to date.\n", gn->name);
		} else if (gn->made == ABORTED) {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
		}
		if (GNode_IsError(gn) && errorNode == NULL)
			errorNode = gn;
	}

	if (errorNode == NULL) {
		GNode *endNode = Targ_GetEndNode();
		Compat_Make(endNode, endNode);
		if (GNode_IsError(endNode))
			errorNode = endNode;
	}

	if (errorNode != NULL) {
		if (DEBUG(GRAPH2))
			Targ_PrintGraph(2);
		else if (DEBUG(GRAPH3))
			Targ_PrintGraph(3);
		PrintOnError(errorNode, "\nStop.\n");
		exit(1);
	}
}
