/*	$NetBSD: make.h,v 1.325 2023/09/10 11:52:29 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)make.h	8.3 (Berkeley) 6/13/95
 */

/*
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
 *	from: @(#)make.h	8.3 (Berkeley) 6/13/95
 */

/*
 * make.h --
 *	The global definitions for make
 */

#ifndef MAKE_MAKE_H
#define MAKE_MAKE_H

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <IntSafe.h>

#define MAKE_ATTR_UNUSED	/* delete */
#define MAKE_ATTR_DEAD		__declspec(noreturn)
#define MAKE_ATTR_PRINTFLIKE	_Printf_format_string_
#define MAKE_ATTR_USE		_Check_return_

#define MAKE_INLINE static inline MAKE_ATTR_UNUSED

/* MAKE_STATIC marks a function that may or may not be inlined. */
#define MAKE_STATIC static MAKE_ATTR_UNUSED

/* path names */
#define _PATH_OBJDIR		"obj"
#define _PATH_DEFSYSMK		"sys.mk"
#define _PATH_DEFSYSPATH	"./mk"

#include "lst.h"
#include "make_malloc.h"
#include "str.h"
#include "hash.h"
#include "make-conf.h"
#include "buf.h"

/*
 * some vendors don't have this --sjg
 */
#if defined(S_IFDIR) && !defined(S_ISDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

/*
 * The typical flow of states is:
 *
 * The direct successful path:
 * UNMADE -> BEINGMADE -> MADE.
 *
 * The direct error path:
 * UNMADE -> BEINGMADE -> ERROR.
 *
 * The successful path when dependencies need to be made first:
 * UNMADE -> DEFERRED -> REQUESTED -> BEINGMADE -> MADE.
 *
 * A node that has dependencies, and one of the dependencies cannot be made:
 * UNMADE -> DEFERRED -> ABORTED.
 *
 * A node that turns out to be up-to-date:
 * UNMADE -> BEINGMADE -> UPTODATE.
 */
typedef enum GNodeMade {
	/* Not examined yet. */
	UNMADE,
	/*
	 * The node has been examined but is not yet ready since its
	 * dependencies have to be made first.
	 */
	DEFERRED,

	/* The node is on the toBeMade list. */
	REQUESTED,

	/*
	 * The node is already being made. Trying to build a node in this
	 * state indicates a cycle in the graph.
	 */
	BEINGMADE,

	/* Was out-of-date and has been made. */
	MADE,
	/* Was already up-to-date, does not need to be made. */
	UPTODATE,
	/*
	 * An error occurred while it was being made. Used only in compat
	 * mode.
	 */
	ERROR,
	/*
	 * The target was aborted due to an error making a dependency. Used
	 * only in compat mode.
	 */
	ABORTED
} GNodeMade;

/*
 * The OP_ constants are used when parsing a dependency line as a way of
 * communicating to other parts of the program the way in which a target
 * should be made.
 *
 * Some of the OP_ constants can be combined, others cannot.
 *
 * See the tests depsrc-*.mk and deptgt-*.mk.
 */
typedef enum GNodeType {
	OP_NONE		= 0,

	/*
	 * The dependency operator ':' is the most common one.  The commands
	 * of this node are executed if any child is out-of-date.
	 */
	OP_DEPENDS	= 1 << 0,
	/*
	 * The dependency operator '!' always executes its commands, even if
	 * its children are up-to-date.
	 */
	OP_FORCE	= 1 << 1,
	/*
	 * The dependency operator '::' behaves like ':', except that it
	 * allows multiple dependency groups to be defined.  Each of these
	 * groups is executed on its own, independently from the others. Each
	 * individual dependency group is called a cohort.
	 */
	OP_DOUBLEDEP	= 1 << 2,

	/* Matches the dependency operators ':', '!' and '::'. */
	OP_OPMASK	= OP_DEPENDS | OP_FORCE | OP_DOUBLEDEP,

	/* Don't care if the target doesn't exist and can't be created. */
	OP_OPTIONAL	= 1 << 3,
	/* Use associated commands for parents. */
	OP_USE		= 1 << 4,
	/*
	 * Target is never out of date, but always execute commands anyway.
	 * Its time doesn't matter, so it has none...sort of.
	 */
	OP_EXEC		= 1 << 5,
	/*
	 * Ignore non-zero exit status from shell commands when creating the
	 * node.
	 */
	OP_IGNORE	= 1 << 6,
	/* Don't remove the target when interrupted. */
	OP_PRECIOUS	= 1 << 7,
	/* Don't echo commands when executed. */
	OP_SILENT	= 1 << 8,
	/*
	 * Target is a recursive make so its commands should always be
	 * executed when it is out of date, regardless of the state of the -n
	 * or -t flags.
	 */
	OP_MAKE		= 1 << 9,
	/*
	 * Target is out-of-date only if any of its children was out-of-date.
	 */
	OP_JOIN		= 1 << 10,
	/* Assume the children of the node have been already made. */
	OP_MADE		= 1 << 11,
	/* Special .BEGIN, .END or .INTERRUPT. */
	OP_SPECIAL	= 1 << 12,
	/* Like .USE, only prepend commands. */
	OP_USEBEFORE	= 1 << 13,
	/*
	 * The node is invisible to its parents. I.e. it doesn't show up in
	 * the parents' local variables (.IMPSRC, .ALLSRC).
	 */
	OP_INVISIBLE	= 1 << 14,
	/*
	 * The node does not become the main target, even if it is the first
	 * target in the first makefile.
	 */
	OP_NOTMAIN	= 1 << 15,
	/* Not a file target; run always. */
	OP_PHONY	= 1 << 16,
	/* Don't search for the file in the path. */
	OP_NOPATH	= 1 << 17,
	/*
	 * In a dependency line "target: source1 .WAIT source2", source1 is
	 * made first, including its children.  Once that is finished,
	 * source2 is made, including its children.  The .WAIT keyword may
	 * appear more than once in a single dependency declaration.
	 */
	OP_WAIT		= 1 << 18,
	/* .NOMETA do not create a .meta file */
	OP_NOMETA	= 1 << 19,
	/* .META we _do_ want a .meta file */
	OP_META		= 1 << 20,
	/* Do not compare commands in .meta file */
	OP_NOMETA_CMP	= 1 << 21,
	/* Possibly a submake node */
	OP_SUBMAKE	= 1 << 22,

	/* Attributes applied by PMake */

	/* The node is a transformation rule, such as ".c.o". */
	OP_TRANSFORM	= 1 << 30,
	/* Target is a member of an archive */
	/* XXX: How does this differ from OP_ARCHV? */
	OP_MEMBER	= 1 << 29,
	/*
	 * The node is a library, its name has the form "-l<libname>".
	 */
	OP_LIB		= 1 << 28,
	/*
	 * The node is an archive member, its name has the form
	 * "archive(member)".
	 */
	/* XXX: How does this differ from OP_MEMBER? */
	OP_ARCHV	= 1 << 27,
	/*
	 * Target has all the commands it should. Used when parsing to catch
	 * multiple command groups for a target.  Only applies to the
	 * dependency operators ':' and '!', but not to '::'.
	 */
	OP_HAS_COMMANDS	= 1 << 26,
	/*
	 * The special command "..." has been seen. All further commands from
	 * this node will be saved on the .END node instead, to be executed
	 * at the very end.
	 */
	OP_SAVE_CMDS	= 1 << 25,
	/*
	 * Already processed by Suff_FindDeps, to find dependencies from
	 * suffix transformation rules.
	 */
	OP_DEPS_FOUND	= 1 << 24,
	/* Node found while expanding .ALLSRC */
	OP_MARK		= 1 << 23
} GNodeType;

typedef struct GNodeFlags {
	/* this target needs to be (re)made */
	bool remake:1;
	/* children of this target were made */
	bool childMade:1;
	/* children don't exist, and we pretend made */
	bool force:1;
	/* Set by Make_ProcessWait() */
	bool doneWait:1;
	/* Build requested by .ORDER processing */
	bool doneOrder:1;
	/* Node created from .depend */
	bool fromDepend:1;
	/* We do it once only */
	bool doneAllsrc:1;
	/* Used by MakePrintStatus */
	bool cycle:1;
	/* Used by MakePrintStatus */
	bool doneCycle:1;
} GNodeFlags;

typedef struct List StringList;
typedef struct ListNode StringListNode;

typedef struct List GNodeList;
typedef struct ListNode GNodeListNode;

typedef struct SearchPath {
	List /* of CachedDir */ dirs;
} SearchPath;

/*
 * A graph node represents a target that can possibly be made, including its
 * relation to other targets and a lot of other details.
 */
typedef struct GNode {
	/* The target's name, such as "clean" or "make.c" */
	char *name;
	/* The unexpanded name of a .USE node */
	char *uname;
	/*
	 * The full pathname of the file belonging to the target.
	 *
	 * XXX: What about .PHONY targets? These don't have an associated
	 * path.
	 */
	char *path;

	/*
	 * The type of operator used to define the sources (see the OP flags
	 * below).
	 *
	 * XXX: This looks like a wild mixture of type and flags.
	 */
	GNodeType type;
	GNodeFlags flags;

	/* The state of processing on this node */
	GNodeMade made;
	/* The number of unmade children */
	int unmade;

	/*
	 * The modification time; 0 means the node does not have a
	 * corresponding file; see GNode_IsOODate.
	 */
	time_t mtime;
	struct GNode *youngestChild;

	/*
	 * The GNodes for which this node is an implied source. May be empty.
	 * For example, when there is an inference rule for .c.o, the node
	 * for file.c has the node for file.o in this list.
	 */
	GNodeList implicitParents;

	/*
	 * The nodes that depend on this one, or in other words, the nodes
	 * for which this is a source.
	 */
	GNodeList parents;
	/* The nodes on which this one depends. */
	GNodeList children;

	/*
	 * .ORDER nodes we need made. The nodes that must be made (if they're
	 * made) before this node can be made, but that do not enter into the
	 * datedness of this node.
	 */
	GNodeList order_pred;
	/*
	 * .ORDER nodes who need us. The nodes that must be made (if they're
	 * made at all) after this node is made, but that do not depend on
	 * this node, in the normal sense.
	 */
	GNodeList order_succ;

	/*
	 * Other nodes of the same name, for targets that were defined using
	 * the '::' dependency operator (OP_DOUBLEDEP).
	 */
	GNodeList cohorts;
	/* The "#n" suffix for this cohort, or "" for other nodes */
	char cohort_num[8];
	/* The number of unmade instances on the cohorts list */
	int unmade_cohorts;
	/*
	 * Pointer to the first instance of a '::' node; only set when on a
	 * cohorts list
	 */
	struct GNode *centurion;

	/* Last time (sequence number) we tried to make this node */
	unsigned int checked_seqno;

	/*
	 * The "local" variables that are specific to this target and this
	 * target only, such as $@, $<, $?.
	 *
	 * Also used for the global variable scopes SCOPE_GLOBAL,
	 * SCOPE_CMDLINE, SCOPE_INTERNAL, which contain variables with
	 * arbitrary names.
	 */
	HashTable /* of Var pointer */ vars;

	/* The commands to be given to a shell to create this target. */
	StringList commands;

	/*
	 * Suffix for the node (determined by Suff_FindDeps and opaque to
	 * everyone but the Suff module)
	 */
	struct Suffix *suffix;

	/* Filename where the GNode got defined, unlimited lifetime */
	const char *fname;
	/* Line number where the GNode got defined, 1-based */
	unsigned lineno;
} GNode;

/*
 * Keep track of whether to include <posix.mk> when parsing the line
 * '.POSIX:'.
 */
extern enum PosixState {
	PS_NOT_YET,
	PS_MAYBE_NEXT_LINE,
	PS_NOW_OR_NEVER,
	PS_TOO_LATE
} posix_state;

/* Error levels for diagnostics during parsing. */
typedef enum ParseErrorLevel {
	/*
	 * Exit when the current top-level makefile has been parsed
	 * completely.
	 */
	PARSE_FATAL = 1,
	/* Print "warning"; may be upgraded to fatal by the -w option. */
	PARSE_WARNING,
	/* Informational, mainly used during development of makefiles. */
	PARSE_INFO
} ParseErrorLevel;

/*
 * Values returned by Cond_EvalLine and Cond_EvalCondition.
 */
typedef enum CondResult {
	CR_TRUE,		/* Parse the next lines */
	CR_FALSE,		/* Skip the next lines */
	CR_ERROR		/* Unknown directive or parse error */
} CondResult;

typedef struct {
	enum GuardKind {
		GK_VARIABLE,
		GK_TARGET
	} kind;
	char *name;
} Guard;

/* Names of the variables that are "local" to a specific target. */
#define TARGET	"@"		/* Target of dependency */
#define OODATE	"?"		/* All out-of-date sources */
#define ALLSRC	">"		/* All sources */
#define IMPSRC	"<"		/* Source implied by transformation */
#define PREFIX	"*"		/* Common prefix */
#define ARCHIVE	"!"		/* Archive in "archive(member)" syntax */
#define MEMBER	"%"		/* Member in "archive(member)" syntax */

/*
 * Global Variables
 */

/* True if every target is precious */
extern bool allPrecious;
/* True if failed targets should be deleted */
extern bool deleteOnError;
/* true while processing .depend */
extern bool doing_depend;
/* .DEFAULT rule */
extern GNode *defaultNode;

/*
 * Variables defined internally by make which should not override those set
 * by makefiles.
 */
extern GNode *SCOPE_INTERNAL;
/* Variables defined in a global scope, e.g in the makefile itself. */
extern GNode *SCOPE_GLOBAL;
/* Variables defined on the command line. */
extern GNode *SCOPE_CMDLINE;

/*
 * Value returned by Var_Parse when an error is encountered. It actually
 * points to an empty string, so naive callers needn't worry about it.
 */
extern char var_Error[];

/* The time at the start of this whole process */
extern time_t now;

/*
 * The list of directories to search when looking for targets (set by the
 * special target .PATH).
 */
extern SearchPath dirSearchPath;
/* Used for .include "...". */
extern SearchPath *parseIncPath;
/*
 * Used for .include <...>, for the built-in sys.mk and for makefiles from
 * the command line arguments.
 */
extern SearchPath *sysIncPath;
/* The default for sysIncPath. */
extern SearchPath *defSysIncPath;

/* Startup directory */
extern char curdir[];
/* The basename of the program name, suffixed with [n] for sub-makes.  */
extern const char *progname;
extern int makelevel;
/* Name of the .depend makefile */
extern char *makeDependfile;

extern GNode *mainNode;

extern DWORD myPid;

#define MAKEFLAGS	".MAKEFLAGS"
#ifndef MAKE_LEVEL_ENV
# define MAKE_LEVEL_ENV	"MAKELEVEL"
#endif

typedef struct DebugFlags {
	bool DEBUG_ARCH:1;
	bool DEBUG_COND:1;
	bool DEBUG_CWD:1;
	bool DEBUG_DIR:1;
	bool DEBUG_ERROR:1;
	bool DEBUG_FOR:1;
	bool DEBUG_GRAPH1:1;
	bool DEBUG_GRAPH2:1;
	bool DEBUG_GRAPH3:1;
	bool DEBUG_HASH:1;
	bool DEBUG_JOB:1;
	bool DEBUG_LOUD:1;
	bool DEBUG_MAKE:1;
	bool DEBUG_META:1;
	bool DEBUG_PARSE:1;
	bool DEBUG_SCRIPT:1;
	bool DEBUG_SHELL:1;
	bool DEBUG_SUFF:1;
	bool DEBUG_TARG:1;
	bool DEBUG_VAR:1;
} DebugFlags;

#define CONCAT(a, b) a##b

#define DEBUG(module) (opts.debug.CONCAT(DEBUG_, module))

void debug_printf(MAKE_ATTR_PRINTFLIKE const char *, ...);

#define DEBUG_IMPL(module, args) \
	do { \
		if (DEBUG(module)) \
			debug_printf args; \
	} while (false)

#define DEBUG0(module, fmt) \
	DEBUG_IMPL(module, (fmt))
#define DEBUG1(module, fmt, arg1) \
	DEBUG_IMPL(module, (fmt, arg1))
#define DEBUG2(module, fmt, arg1, arg2) \
	DEBUG_IMPL(module, (fmt, arg1, arg2))
#define DEBUG3(module, fmt, arg1, arg2, arg3) \
	DEBUG_IMPL(module, (fmt, arg1, arg2, arg3))
#define DEBUG4(module, fmt, arg1, arg2, arg3, arg4) \
	DEBUG_IMPL(module, (fmt, arg1, arg2, arg3, arg4))
#define DEBUG5(module, fmt, arg1, arg2, arg3, arg4, arg5) \
	DEBUG_IMPL(module, (fmt, arg1, arg2, arg3, arg4, arg5))

typedef enum PrintVarsMode {
	PVM_NONE,
	PVM_UNEXPANDED,
	PVM_EXPANDED
} PrintVarsMode;

/* Command line options */
typedef struct CmdOpts {
	/* -B: whether we are make compatible */
	bool compatMake;

	/*
	 * -d: debug control: There is one bit per module.  It is up to the
	 * module what debug information to print.
	 */
	DebugFlags debug;

	/* -df: debug output is written here - default stderr */
	FILE *debug_file;

	/*
	 * -dL: lint mode
	 *
	 * Runs make in strict mode, with additional checks and better error
	 * handling.
	 */
	bool strict;

	/* -dV: for the -V option, print unexpanded variable values */
	bool debugVflag;

	/* -e: check environment variables before global variables */
	bool checkEnvFirst;

	/* -f: the makefiles to read */
	StringList makefiles;

	/* -i: if true, ignore all errors from shell commands */
	bool ignoreErrors;

	/*
	 * -j: the maximum number of jobs that can run in parallel; this is
	 * coordinated with the submakes
	 */
	int maxJobs;

	/*
	 * -k: if true and an error occurs while making a node, continue
	 * making nodes that do not depend on the erroneous node
	 */
	bool keepgoing;

	/* -N: execute no commands from the targets */
	bool noRecursiveExecute;

	/* -n: execute almost no commands from the targets */
	bool noExecute;

	/*
	 * -q: if true, do not really make anything, just see if the targets
	 * are out-of-date
	 */
	bool query;

	/* -r: raw mode, do not load the builtin rules. */
	bool noBuiltins;

	/* -s: don't echo the shell commands before executing them */
	bool silent;

	/*
	 * -t: touch the targets if they are out-of-date, but don't actually
	 * make them
	 */
	bool touch;

	/* -[Vv]: print expanded or unexpanded selected variables */
	PrintVarsMode printVars;
	/* -[Vv]: the variables to print */
	StringList variables;

	/* -W: if true, makefile parsing warnings are treated as errors */
	bool parseWarnFatal;

	/* -w: print 'Entering' and 'Leaving' for submakes */
	bool enterFlag;

	/*
	 * -X: if true, do not export variables set on the command line to
	 * the environment.
	 */
	bool varNoExportEnv;

	/*
	 * The target names specified on the command line. Used to resolve
	 * .if make(...) statements.
	 */
	StringList create;

	/*
	 * Randomize the order in which the targets from toBeMade are made,
	 * to catch undeclared dependencies.
	 */
	bool randomizeTargets;
} CmdOpts;

extern CmdOpts opts;
extern bool forceJobs;

/* arch.c */
void Arch_Init(void);
void Arch_End(void);

bool Arch_ParseArchive(char **, GNodeList *, GNode *);
void Arch_Touch(GNode *);
void Arch_TouchLib(GNode *);
void Arch_UpdateMTime(GNode *);
void Arch_UpdateMemberMTime(GNode *);
void Arch_FindLib(GNode *, SearchPath *);
bool MAKE_ATTR_USE Arch_LibOODate(GNode *);
bool MAKE_ATTR_USE Arch_IsLib(GNode *);

/* compat.c */
bool Compat_RunCommand(const char *, GNode *, StringListNode *);
void Compat_MakeAll(GNodeList *);
void Compat_Make(GNode *, GNode *);

/* cond.c */
extern unsigned int cond_depth;
CondResult MAKE_ATTR_USE Cond_EvalCondition(const char *);
CondResult MAKE_ATTR_USE Cond_EvalLine(const char *);
Guard * MAKE_ATTR_USE Cond_ExtractGuard(const char *);
void Cond_EndFile(void);

/* dir.c; see also dir.h */

/* Replacment for if(p == '/') */
#define isAbs(p) (((p) <= 'Z' && (p) >= 'A')||((p) <= 'z' && (p) >= 'a'))

MAKE_INLINE char * MAKE_ATTR_USE
lastSlash(const char *pathname)
{
	char *last1 = strrchr(pathname, '/');
	char *last2 = strrchr(pathname, '\\');

	if(last1 == last2)
		return NULL;

	return last1 > last2 ? last1 : last2;
}

/* Replace all instances of '\' with '/' */
MAKE_INLINE void
replaceSlash(char* p)
{
	if ((p = strchr(p, '\\')) != NULL)
		for (; *p != '\0'; p++)
			if (*p == '\\')
				*p = '/';
}

MAKE_INLINE const char * MAKE_ATTR_USE
str_basename(const char *pathname)
{
	const char *s = lastSlash(pathname);
	return s != NULL ? s + 1 : pathname;
}

MAKE_INLINE SearchPath * MAKE_ATTR_USE
SearchPath_New(void)
{
	SearchPath *path = bmake_malloc(sizeof *path);
	Lst_Init(&path->dirs);
	return path;
}

void SearchPath_Free(SearchPath *);

/* for.c */
struct ForLoop;
int MAKE_ATTR_USE For_Eval(const char *);
bool MAKE_ATTR_USE For_Accum(const char *, int *);
void For_Run(unsigned, unsigned);
bool For_NextIteration(struct ForLoop *, Buffer *);
char *ForLoop_Details(const struct ForLoop *);
void ForLoop_Free(struct ForLoop *);
void For_Break(struct ForLoop *);

/* main.c */

/*
 * If a process's output pipe is full, any writes to it block.
 * While the process is running, we check if the pipe is full
 * every PROCESSWAIT ms.
 */
#define PROCESSWAIT 100

/* pipe size */
#define PIPESZ 4096

void Main_ParseArgLine(const char *);
char * MAKE_ATTR_USE Cmd_Exec(const char *, char **);
void Error(MAKE_ATTR_PRINTFLIKE const char *, ...);
void MAKE_ATTR_DEAD Fatal(MAKE_ATTR_PRINTFLIKE const char *, ...);
void MAKE_ATTR_DEAD Punt(MAKE_ATTR_PRINTFLIKE const char *, ...);
void MAKE_ATTR_DEAD DieHorribly(void);
void MAKE_ATTR_DEAD Finish(int);
int MAKE_ATTR_USE unlink_file(const char *);
char * MAKE_ATTR_USE getTmpdir(void);
bool MAKE_ATTR_USE ParseBoolean(const char *, bool);
const char *cached_realpath(const char *, char *);
bool GetBooleanExpr(const char *, bool);

/* parse.c */
void Parse_Init(void);
void Parse_End(void);

void PrintLocation(FILE *, bool, const GNode *);
void PrintStackTrace(bool);
void Parse_Error(ParseErrorLevel, MAKE_ATTR_PRINTFLIKE const char *, ...);
bool MAKE_ATTR_USE Parse_VarAssign(const char *, bool, GNode *);
void Parse_File(const char *, int);
void Parse_PushInput(const char *, unsigned, unsigned, Buffer,
		     struct ForLoop *);
void Parse_MainName(GNodeList *);
int MAKE_ATTR_USE Parse_NumErrors(void);
unsigned int MAKE_ATTR_USE CurFile_CondMinDepth(void);
void Parse_GuardElse(void);
void Parse_GuardEndif(void);


/* suff.c */
void Suff_Init(void);
void Suff_End(void);

void Suff_ClearSuffixes(void);
bool MAKE_ATTR_USE Suff_IsTransform(const char *);
GNode *Suff_AddTransform(const char *);
void Suff_EndTransform(GNode *);
void Suff_AddSuffix(const char *);
SearchPath * MAKE_ATTR_USE Suff_GetPath(const char *);
void Suff_ExtendPaths(void);
void Suff_AddInclude(const char *);
void Suff_AddLib(const char *);
void Suff_FindDeps(GNode *);
SearchPath * MAKE_ATTR_USE Suff_FindPath(GNode *);
void Suff_SetNull(const char *);
void Suff_PrintAll(void);
char * MAKE_ATTR_USE Suff_NamesStr(void);

/* targ.c */
void Targ_Init(void);
void Targ_End(void);

void Targ_Stats(void);
GNodeList * MAKE_ATTR_USE Targ_List(void);
GNode * MAKE_ATTR_USE GNode_New(const char *);
GNode * MAKE_ATTR_USE Targ_FindNode(const char *);
GNode * MAKE_ATTR_USE Targ_GetNode(const char *);
GNode * MAKE_ATTR_USE Targ_NewInternalNode(const char *);
GNode *Targ_GetEndNode(void);
void Targ_FindList(GNodeList *, StringList *);
void Targ_PrintCmds(GNode *);
void Targ_PrintNode(GNode *, int);
void Targ_PrintNodes(GNodeList *, int);
const char * MAKE_ATTR_USE Targ_FmtTime(time_t);
void Targ_PrintType(GNodeType);
void Targ_PrintGraph(int);
void Targ_Propagate(void);
const char * MAKE_ATTR_USE GNodeMade_Name(GNodeMade);

/* var.c */
void Var_Init(void);
void Var_End(void);

typedef enum VarEvalMode {

	/*
	 * Only parse the expression but don't evaluate any part of it.
	 *
	 * TODO: Document what Var_Parse and Var_Subst return in this mode.
	 *  As of 2021-03-15, they return unspecified, inconsistent results.
	 */
	VARE_PARSE_ONLY,

	/*
	 * Parse text in which '${...}' and '$(...)' are not parsed as
	 * subexpressions (with all their individual escaping rules) but
	 * instead simply as text with balanced '${}' or '$()'.  Other '$'
	 * are copied verbatim.
	 */
	VARE_PARSE_BALANCED,

	/* Parse and evaluate the expression. */
	VARE_WANTRES,

	/*
	 * Parse and evaluate the expression.  It is an error if a
	 * subexpression evaluates to undefined.
	 */
	VARE_UNDEFERR,

	/*
	 * Parse and evaluate the expression.  Keep '$$' as '$$' instead of
	 * reducing it to a single '$'.  Subexpressions that evaluate to
	 * undefined expand to an empty string.
	 *
	 * Used in variable assignments using the ':=' operator.  It allows
	 * multiple such assignments to be chained without accidentally
	 * expanding '$$file' to '$file' in the first assignment and
	 * interpreting it as '${f}' followed by 'ile' in the next assignment.
	 */
	VARE_EVAL_KEEP_DOLLAR,

	/*
	 * Parse and evaluate the expression.  Keep undefined variables as-is
	 * instead of expanding them to an empty string.
	 *
	 * Example for a ':=' assignment:
	 *	CFLAGS = $(.INCLUDES)
	 *	CFLAGS := -I.. $(CFLAGS)
	 *	# If .INCLUDES (an undocumented special variable, by the
	 *	# way) is still undefined, the updated CFLAGS becomes
	 *	# "-I.. $(.INCLUDES)".
	 */
	VARE_EVAL_KEEP_UNDEF,

	/*
	 * Parse and evaluate the expression.  Keep '$$' as '$$' and preserve
	 * undefined subexpressions.
	 */
	VARE_KEEP_DOLLAR_UNDEF
} VarEvalMode;

typedef enum VarSetFlags {
	VAR_SET_NONE		= 0,

	/* do not export */
	VAR_SET_NO_EXPORT	= 1 << 0,

	/*
	 * Make the variable read-only. No further modification is possible,
	 * except for another call to Var_Set with the same flag. See the
	 * special targets '.NOREADONLY' and '.READONLY'.
	 */
	VAR_SET_READONLY	= 1 << 1
} VarSetFlags;

typedef enum VarExportMode {
	/* .export-env */
	VEM_ENV,
	/* .export: Initial export or update an already exported variable. */
	VEM_PLAIN,
	/* .export-literal: Do not expand the variable value. */
	VEM_LITERAL
} VarExportMode;

void Var_Delete(GNode *, const char *);
void Var_Undef(const char *);
void Var_Set(GNode *, const char *, const char *);
void Var_SetExpand(GNode *, const char *, const char *);
void Var_SetWithFlags(GNode *, const char *, const char *, VarSetFlags);
void Var_Append(GNode *, const char *, const char *);
void Var_AppendExpand(GNode *, const char *, const char *);
bool MAKE_ATTR_USE Var_Exists(GNode *, const char *);
bool MAKE_ATTR_USE Var_ExistsExpand(GNode *, const char *);
FStr MAKE_ATTR_USE Var_Value(GNode *, const char *);
const char * MAKE_ATTR_USE GNode_ValueDirect(GNode *, const char *);
FStr Var_Parse(const char **, GNode *, VarEvalMode);
char *Var_Subst(const char *, GNode *, VarEvalMode);
void Var_Expand(FStr *, GNode *, VarEvalMode);
void Var_Stats(void);
void Var_Dump(GNode *);
void Var_ReexportVars(void);
void Var_Export(VarExportMode, const char *);
void Var_ExportVars(const char *);
void Var_UnExport(bool, const char *);
void Var_ReadOnly(const char *, bool);

void Global_Set(const char *, const char *);
void Global_Append(const char *, const char *);
void Global_Delete(const char *);
void Global_Set_ReadOnly(const char *, const char *);

/* util.c */
int setenv(const char *, const char *, int);
int unsetenv(const char *);
uint32_t random(void);
void err(int, MAKE_ATTR_PRINTFLIKE const char *, ...);
void errx(int, MAKE_ATTR_PRINTFLIKE const char *, ...);
void warn(MAKE_ATTR_PRINTFLIKE const char *, ...);
void warnx(MAKE_ATTR_PRINTFLIKE const char *, ...);

/* make.c */
void GNode_UpdateYoungestChild(GNode *, GNode *);
bool MAKE_ATTR_USE GNode_IsOODate(GNode *);
void Make_ExpandUse(GNodeList *);
time_t MAKE_ATTR_USE Make_Recheck(GNode *);
void Make_HandleUse(GNode *, GNode *);
void Make_Update(GNode *);
void GNode_SetLocalVars(GNode *);
bool Make_Run(GNodeList *);
bool MAKE_ATTR_USE shouldDieQuietly(GNode *, int);
void PrintOnError(GNode *, const char *);
void Main_ExportMAKEFLAGS(bool);
bool Main_SetObjdir(bool, MAKE_ATTR_PRINTFLIKE const char *, ...);
const char *strerr(DWORD e);
int str2Lst_Append(StringList *, char *);
void GNode_FprintDetails(FILE *, const char *, const GNode *, const char *);
bool MAKE_ATTR_USE GNode_ShouldExecute(GNode *gn);

/* message.c */
void Msg_Init(void (*)(void), void (*)(void));
void Msg_End(void);

size_t strlcpy(char *, const char *, size_t);

/* See if the node was seen on the left-hand side of a dependency operator. */
MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsTarget(const GNode *gn)
{
	return (gn->type & OP_OPMASK) != OP_NONE;
}

MAKE_INLINE const char * MAKE_ATTR_USE
GNode_Path(const GNode *gn)
{
	return gn->path != NULL ? gn->path : gn->name;
}

MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsWaitingFor(const GNode *gn)
{
	return gn->flags.remake && gn->made <= REQUESTED;
}

MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsReady(const GNode *gn)
{
	return gn->made > DEFERRED;
}

MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsDone(const GNode *gn)
{
	return gn->made >= MADE;
}

MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsError(const GNode *gn)
{
	return gn->made == ERROR || gn->made == ABORTED;
}

MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsMainCandidate(const GNode *gn)
{
	return (gn->type & (OP_NOTMAIN | OP_USE | OP_USEBEFORE |
			    OP_EXEC | OP_TRANSFORM)) == 0;
}

/* Return whether the target file should be preserved on interrupt. */
MAKE_INLINE bool MAKE_ATTR_USE
GNode_IsPrecious(const GNode *gn)
{
	/* XXX: Why are '::' targets precious? */
	return allPrecious || gn->type & (OP_PRECIOUS | OP_DOUBLEDEP);
}

MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarTarget(GNode *gn) { return GNode_ValueDirect(gn, TARGET); }
MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarOodate(GNode *gn) { return GNode_ValueDirect(gn, OODATE); }
MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarAllsrc(GNode *gn) { return GNode_ValueDirect(gn, ALLSRC); }
MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarImpsrc(GNode *gn) { return GNode_ValueDirect(gn, IMPSRC); }
MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarPrefix(GNode *gn) { return GNode_ValueDirect(gn, PREFIX); }
MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarArchive(GNode *gn) { return GNode_ValueDirect(gn, ARCHIVE); }
MAKE_INLINE const char * MAKE_ATTR_USE
GNode_VarMember(GNode *gn) { return GNode_ValueDirect(gn, MEMBER); }

MAKE_INLINE void * MAKE_ATTR_USE
UNCONST(const void *ptr)
{
	void *ret;
	memcpy(&ret, &ptr, sizeof(ret));
	return ret;
}

#define MAXPATHLEN	_MAX_PATH
#define PATH_MAX	_MAX_PATH

MAKE_INLINE bool MAKE_ATTR_USE
ch_isalnum(char ch) { return isalnum((unsigned char)ch) != 0; }
MAKE_INLINE bool MAKE_ATTR_USE
ch_isalpha(char ch) { return isalpha((unsigned char)ch) != 0; }
MAKE_INLINE bool MAKE_ATTR_USE
ch_isdigit(char ch) { return isdigit((unsigned char)ch) != 0; }
MAKE_INLINE bool MAKE_ATTR_USE
ch_islower(char ch) { return islower((unsigned char)ch) != 0; }
MAKE_INLINE bool MAKE_ATTR_USE
ch_isspace(char ch) { return isspace((unsigned char)ch) != 0; }
MAKE_INLINE bool MAKE_ATTR_USE
ch_isupper(char ch) { return isupper((unsigned char)ch) != 0; }
MAKE_INLINE char MAKE_ATTR_USE
ch_tolower(char ch) { return (char)tolower((unsigned char)ch); }
MAKE_INLINE char MAKE_ATTR_USE
ch_toupper(char ch) { return (char)toupper((unsigned char)ch); }

MAKE_INLINE void
cpp_skip_whitespace(const char **pp)
{
	while (ch_isspace(**pp))
		(*pp)++;
}

MAKE_INLINE void
cpp_skip_hspace(const char **pp)
{
	while (**pp == ' ' || **pp == '\t')
		(*pp)++;
}

MAKE_INLINE bool
cpp_skip_string(const char **pp, const char *s)
{
	const char *p = *pp;
	while (*p == *s && *s != '\0')
		p++, s++;
	if (*s == '\0')
		*pp = p;
	return *s == '\0';
}

MAKE_INLINE void
pp_skip_whitespace(char **pp)
{
	while (ch_isspace(**pp))
		(*pp)++;
}

MAKE_INLINE void
pp_skip_hspace(char **pp)
{
	while (**pp == ' ' || **pp == '\t')
		(*pp)++;
}

#endif
