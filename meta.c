/*      $NetBSD: meta.c,v 1.206 2023/08/19 00:09:17 sjg Exp $ */

/*
 * Implement 'meta' mode.
 * Adapted from John Birrell's patches to FreeBSD make.
 * --sjg
 */
/*
 * Copyright (c) 2009-2016, Juniper Networks, Inc.
 * Portions Copyright (c) 2009, John Birrell.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if defined(USE_META)

#include <direct.h>
#include <sys/stat.h>

#include "make.h"
#include "dir.h"
#include "job.h"

static BuildMon Mybm;			/* for compat */
static StringList metaBailiwick = LST_INIT; /* our scope of control */
static char *metaBailiwickStr;		/* string storage for the list */
static StringList metaIgnorePaths = LST_INIT; /* paths we deliberately ignore */
static char *metaIgnorePathsStr;	/* string storage for the list */

#ifndef MAKE_META_IGNORE_PATHS
#define MAKE_META_IGNORE_PATHS ".MAKE.META.IGNORE_PATHS"
#endif
#ifndef MAKE_META_IGNORE_PATTERNS
#define MAKE_META_IGNORE_PATTERNS ".MAKE.META.IGNORE_PATTERNS"
#endif
#ifndef MAKE_META_IGNORE_FILTER
#define MAKE_META_IGNORE_FILTER ".MAKE.META.IGNORE_FILTER"
#endif
#ifndef MAKE_META_CMP_FILTER
#define MAKE_META_CMP_FILTER ".MAKE.META.CMP_FILTER"
#endif

char *dirname(char *);

bool useMeta = false;
static bool writeMeta = false;
static bool metaMissing = false;	/* oodate if missing */
static bool metaEnv = false;		/* don't save env unless asked */
static bool metaVerbose = false;
static bool metaIgnoreCMDs = false;	/* ignore CMDs in .meta files */
static bool metaIgnorePatterns = false; /* do we need to do pattern matches */
static bool metaIgnoreFilter = false;	/* do we have more complex filtering? */
static bool metaCmpFilter = false;	/* do we have CMP_FILTER ? */
static bool metaCurdirOk = false;	/* write .meta in .CURDIR Ok? */
static bool metaSilent = false;		/* if we have a .meta be SILENT */


#define MAKE_META_PREFIX	".MAKE.META.PREFIX"

#define N2U(n, u)   (((n) + ((u) - 1)) / (u))
#define ROUNDUP(n, u)   (N2U((n), (u)) * (u))
#define strsep(s, d) stresep((s), (d), '\0')

char * stresep(char **, const char *, int);

/*
 * when realpath() fails,
 * we use this, to clean up ./ and ../
 */
static void
eat_dots(char *buf)
{
	char *p;

	/* Make life easier by replacing all backslashes. */
	replaceSlash(buf);

	while ((p = strstr(buf, "/./")) != NULL)
	memmove(p, p + 2, strlen(p + 2) + 1);

	while ((p = strstr(buf, "/../")) != NULL) {
	char *p2 = p + 3;
	if (p > buf) {
		do {
		p--;
		} while (p > buf && *p != '/');
	}
	if (*p == '/')
		memmove(p, p2, strlen(p2) + 1);
	else
		return;		/* can't happen? */
	}
}

static char *
meta_name(char *mname, size_t mnamelen,
	  const char *dname,
	  const char *tname,
	  const char *cwd)
{
	char buf[MAXPATHLEN];
	char *rp, *cp;
	const char *tname_base;
	char *tp;
	char *dtp;
	size_t ldname;

	/*
	 * Weed out relative paths from the target file name.
	 * We have to be careful though since if target is a
	 * symlink, the result will be unstable.
	 * So we use realpath() just to get the dirname, and leave the
	 * basename as given to us.
	 */
	if ((tname_base = lastSlash(tname)) != NULL) {
	if (cached_realpath(tname, buf) != NULL) {
		if ((rp = lastSlash(buf)) != NULL) {
		rp++;
		tname_base++;
		if (strcmp(tname_base, rp) != 0)
			strlcpy(rp, tname_base, sizeof buf - (size_t)(rp - buf));
		}
		tname = buf;
	} else {
		/*
		 * We likely have a directory which is about to be made.
		 * We pretend realpath() succeeded, to have a chance
		 * of generating the same meta file name that we will
		 * next time through.
		 */
		if (isAbs(tname[0])) {
		strlcpy(buf, tname, sizeof buf);
		} else {
		snprintf(buf, sizeof buf, "%s\\%s", cwd, tname);
		}
		eat_dots(buf);
		tname = buf;
	}
	}
	/* on some systems dirname may modify its arg */
	tp = bmake_strdup(tname);
	dtp = dirname(tp);
	if (strcmp(dname, dtp) == 0) {
	if (snprintf(mname, mnamelen, "%s.meta", tname) >= (int)mnamelen)
		mname[mnamelen - 1] = '\0';
	} else {
	int x;

	ldname = strlen(dname);
	if (strncmp(dname, dtp, ldname) == 0 && isAbs(dtp[ldname]))
		x = snprintf(mname, mnamelen, "%s\\%s.meta", dname, &tname[ldname+1]);
	else
		x = snprintf(mname, mnamelen, "%s\\%s.meta", dname, tname);
	if (x >= (int)mnamelen)
		mname[mnamelen - 1] = '\0';
	/*
	 * Replace path separators in the file name after the
	 * current object directory path.
	 */
	cp = mname + strlen(dname) + 1;

	while (*cp != '\0') {
		if (*cp == '/' ||
			*cp == '\\')
		*cp = '_';
		cp++;
	}
	}
	free(tp);
	return mname;
}

/*
 * Return true if running ${.MAKE}
 * Bypassed if target is flagged .MAKE
 */
static bool
is_submake(const char *cmd, GNode *gn)
{
	static const char *p_make = NULL;
	static size_t p_len;
	char *mp = NULL;
	const char *cp2;
	bool rc = false;

	if (p_make == NULL) {
	p_make = Var_Value(gn, ".MAKE").str;
	p_len = strlen(p_make);
	}
	if (strchr(cmd, '$') != NULL) {
	mp = Var_Subst(cmd, gn, VARE_WANTRES);
	/* TODO: handle errors */
	cmd = mp;
	}
	cp2 = strstr(cmd, p_make);
	if (cp2 != NULL) {
	switch (cp2[p_len]) {
	case '\0':
	case ' ':
	case '\t':
	case '\n':
		rc = true;
		break;
	}
	if (cp2 > cmd && rc) {
		switch (cp2[-1]) {
		case ' ':
		case '\t':
		case '\n':
		break;
		default:
		rc = false;		/* no match */
		break;
		}
	}
	}
	free(mp);
	return rc;
}

static bool
any_is_submake(GNode *gn)
{
	StringListNode *ln;

	for (ln = gn->commands.first; ln != NULL; ln = ln->next)
	if (is_submake(ln->datum, gn))
		return true;
	return false;
}

static void
printCMD(const char *ucmd, FILE *fp, GNode *gn)
{
	FStr xcmd = FStr_InitRefer(ucmd);

	Var_Expand(&xcmd, gn, VARE_WANTRES);
	fprintf(fp, "CMD %s\n", xcmd.str);
	FStr_Done(&xcmd);
}

static void
printCMDs(GNode *gn, FILE *fp)
{
	StringListNode *ln;

	for (ln = gn->commands.first; ln != NULL; ln = ln->next)
	printCMD(ln->datum, fp, gn);
}

/*
 * Certain node types never get a .meta file
 */
#define SKIP_META_TYPE(flag, str) do { \
	if ((gn->type & (flag))) { \
	if (verbose) \
		debug_printf("Skipping meta for %s: .%s\n", gn->name, str); \
	return false; \
	} \
} while (false)


/*
 * Do we need/want a .meta file ?
 */
static bool
meta_needed(GNode *gn, const char *dname,
		char *objdir_realpath, bool verbose)
{
	struct cached_stat cst;

	if (verbose)
	verbose = DEBUG(META);

	/* This may be a phony node which we don't want meta data for... */
	/* Skip .meta for .BEGIN, .END, .ERROR etc as well. */
	/* Or it may be explicitly flagged as .NOMETA */
	SKIP_META_TYPE(OP_NOMETA, "NOMETA");
	/* Unless it is explicitly flagged as .META */
	if (!(gn->type & OP_META)) {
	SKIP_META_TYPE(OP_PHONY, "PHONY");
	SKIP_META_TYPE(OP_SPECIAL, "SPECIAL");
	SKIP_META_TYPE(OP_MAKE, "MAKE");
	}

	/* Check if there are no commands to execute. */
	if (Lst_IsEmpty(&gn->commands)) {
	if (verbose)
		debug_printf("Skipping meta for %s: no commands\n", gn->name);
	return false;
	}
	if ((gn->type & (OP_META|OP_SUBMAKE)) == OP_SUBMAKE) {
	/* OP_SUBMAKE is a bit too aggressive */
	if (any_is_submake(gn)) {
		DEBUG1(META, "Skipping meta for %s: .SUBMAKE\n", gn->name);
		return false;
	}
	}

	/* The object directory may not exist. Check it.. */
	if (cached_stat(dname, &cst) != 0) {
	if (verbose)
		debug_printf("Skipping meta for %s: no .OBJDIR\n", gn->name);
	return false;
	}

	/* make sure these are canonical */
	if (cached_realpath(dname, objdir_realpath) != NULL)
	dname = objdir_realpath;

	/* If we aren't in the object directory, don't create a meta file. */
	if (!metaCurdirOk && strcmp(curdir, dname) == 0) {
	if (verbose)
		debug_printf("Skipping meta for %s: .OBJDIR == .CURDIR\n",
			 gn->name);
	return false;
	}
	return true;
}


static FILE *
meta_create(BuildMon *pbm, GNode *gn)
{
	FILE *fp;
	char buf[MAXPATHLEN];
	char objdir_realpath[MAXPATHLEN];
	char **ptr;
	FStr dname;
	const char *tname;
	char *fname;
	const char *cp;

	fp = NULL;

	dname = Var_Value(gn, ".OBJDIR");
	tname = GNode_VarTarget(gn);

	/* if this succeeds objdir_realpath is realpath of dname */
	if (!meta_needed(gn, dname.str, objdir_realpath, true))
	goto out;
	dname.str = objdir_realpath;

	if (metaVerbose) {
	/* Describe the target we are building */
	char *mp = Var_Subst("${" MAKE_META_PREFIX "}", gn, VARE_WANTRES);
	/* TODO: handle errors */
	if (mp[0] != '\0')
		fprintf(stdout, "%s\n", mp);
	free(mp);
	}
	/* Get the basename of the target */
	cp = str_basename(tname);

	fflush(stdout);

	if (!writeMeta)
	/* Don't create meta data. */
	goto out;

	fname = meta_name(pbm->meta_fname, sizeof pbm->meta_fname,
			  dname.str, tname, objdir_realpath);

#ifdef DEBUG_META_MODE
	DEBUG1(META, "meta_create: %s\n", fname);
#endif

	if ((fp = fopen(fname, "w")) == NULL)
	Punt("could not open meta file '%s': %s", fname, strerror(errno));

	fprintf(fp, "# Meta data file %s\n", fname);

	printCMDs(gn, fp);

	fprintf(fp, "CWD %s\n", _getcwd(buf, sizeof buf));
	fprintf(fp, "TARGET %s\n", tname);
	cp = GNode_VarOodate(gn);
	if (cp != NULL && *cp != '\0') {
	fprintf(fp, "OODATE %s\n", cp);
	}
	if (metaEnv) {
	for (ptr = environ; *ptr != NULL; ptr++)
		fprintf(fp, "ENV %s\n", *ptr);
	}

	fprintf(fp, "-- command output --\n");
	if (fflush(fp) != 0)
	Punt("cannot write expanded command to meta file: %s",
		strerror(errno));

	Global_Append(".MAKE.META.FILES", fname);
	Global_Append(".MAKE.META.CREATED", fname);

	gn->type |= OP_META;		/* in case anyone wants to know */
	if (metaSilent) {
		gn->type |= OP_SILENT;
	}
 out:
	FStr_Done(&dname);

	return fp;
}

static bool
boolValue(const char *s)
{
	switch (*s) {
	case '0':
	case 'N':
	case 'n':
	case 'F':
	case 'f':
	return false;
	}
	return true;
}


#define get_mode_bf(bf, token) \
	if ((cp = strstr(make_mode, token)) != NULL) \
	bf = boolValue(cp + sizeof (token) - 1)

/*
 * Initialization we need after reading makefiles.
 */
void
meta_mode_init(const char *make_mode)
{
	static bool once = false;
	const char *cp;

	useMeta = true;
	writeMeta = true;

	if (make_mode != NULL) {
	if (strstr(make_mode, "env") != NULL)
		metaEnv = true;
	if (strstr(make_mode, "verb") != NULL)
		metaVerbose = true;
	if (strstr(make_mode, "read") != NULL)
		writeMeta = false;
	if (strstr(make_mode, "ignore-cmd") != NULL)
		metaIgnoreCMDs = true;
	get_mode_bf(metaCurdirOk, "curdirok=");
	get_mode_bf(metaMissing, "missing-meta=");
	get_mode_bf(metaSilent, "silent=");
	}
	if (metaVerbose && !Var_Exists(SCOPE_GLOBAL, MAKE_META_PREFIX)) {
	/*
	 * The default value for MAKE_META_PREFIX
	 * prints the absolute path of the target.
	 * This works be cause :H will generate '.' if there is no /
	 * and :tA will resolve that to cwd.
	 */
	Global_Set(MAKE_META_PREFIX,
		"Building ${.TARGET:H:tA}\\${.TARGET:T}");
	}
	if (once)
	return;
	once = true;
	memset(&Mybm, 0, sizeof Mybm);
	/*
	 * We consider ourselves master of all within ${.MAKE.META.BAILIWICK}
	 */
	metaBailiwickStr = Var_Subst("${.MAKE.META.BAILIWICK:O:u:tA}",
				 SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */
	str2Lst_Append(&metaBailiwick, metaBailiwickStr);
	/*
	 * We ignore any paths that start with ${.MAKE.META.IGNORE_PATHS}
	 */
	metaIgnorePathsStr = Var_Subst("${" MAKE_META_IGNORE_PATHS ":O:u:tA}",
				   SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */
	str2Lst_Append(&metaIgnorePaths, metaIgnorePathsStr);

	/*
	 * We ignore any paths that match ${.MAKE.META.IGNORE_PATTERNS}
	 */
	metaIgnorePatterns = Var_Exists(SCOPE_GLOBAL, MAKE_META_IGNORE_PATTERNS);
	metaIgnoreFilter = Var_Exists(SCOPE_GLOBAL, MAKE_META_IGNORE_FILTER);
	metaCmpFilter = Var_Exists(SCOPE_GLOBAL, MAKE_META_CMP_FILTER);
}

MAKE_INLINE BuildMon *
BM(Job *job)
{

	return ((job != NULL) ? &job->bm : &Mybm);
}

/*
 * In each case below we allow for job==NULL
 */
void
meta_job_start(Job *job, GNode *gn)
{
	BuildMon *pbm;

	pbm = BM(job);
	pbm->mfp = meta_create(pbm, gn);
}

void
meta_job_error(Job *job, GNode *gn, bool ignerr, int status)
{
	char cwd[MAXPATHLEN];
	BuildMon *pbm;

	pbm = BM(job);
	if (job != NULL && gn == NULL)
		gn = job->node;
	if (pbm->mfp != NULL) {
	fprintf(pbm->mfp, "\n*** Error code %d%s\n",
		status, ignerr ? "(ignored)" : "");
	}
	if (gn != NULL)
	Global_Set(".ERROR_TARGET", GNode_Path(gn));
	if (getcwd(cwd, sizeof cwd) == NULL)
	Punt("cannot get cwd: %s", strerror(errno));

	Global_Set(".ERROR_CWD", cwd);
	if (pbm->meta_fname[0] != '\0') {
	Global_Set(".ERROR_META_FILE", pbm->meta_fname);
	}
	meta_job_finish(job);
}

void
meta_job_output(Job *job, char *cp, const char *nl)
{
	BuildMon *pbm;

	pbm = BM(job);
	if (pbm->mfp != NULL) {
	if (metaVerbose) {
		static char *meta_prefix = NULL;
		static size_t meta_prefix_len;

		if (meta_prefix == NULL) {
		char *cp2;

		meta_prefix = Var_Subst("${" MAKE_META_PREFIX "}",
					SCOPE_GLOBAL, VARE_WANTRES);
		/* TODO: handle errors */
		if ((cp2 = strchr(meta_prefix, '$')) != NULL)
			meta_prefix_len = (size_t)(cp2 - meta_prefix);
		else
			meta_prefix_len = strlen(meta_prefix);
		}
		if (strncmp(cp, meta_prefix, meta_prefix_len) == 0) {
		cp = strchr(cp + 1, '\n');
		if (cp == NULL)
			return;
		cp++;
		}
	}
	fprintf(pbm->mfp, "%s%s", cp, nl);
	}
}

int
meta_cmd_finish(void *pbmp)
{
	int error = 0;
	BuildMon *pbm = pbmp;

	if (pbm == NULL)
	pbm = &Mybm;

	fprintf(pbm->mfp, "\n");	/* ensure end with newline */
	return error;
}

int
meta_job_finish(Job *job)
{
	BuildMon *pbm;
	int error = 0;
	int x;

	pbm = BM(job);
	if (pbm->mfp != NULL) {
	error = meta_cmd_finish(pbm);
	x = fclose(pbm->mfp);
	if (error == 0 && x != 0)
		error = errno;
	pbm->mfp = NULL;
	pbm->meta_fname[0] = '\0';
	}
	return error;
}

void
meta_finish(void)
{
	Lst_Done(&metaBailiwick);
	free(metaBailiwickStr);
	Lst_Done(&metaIgnorePaths);
	free(metaIgnorePathsStr);
}

/*
 * Fetch a full line from fp - growing bufp if needed
 * Return length in bufp.
 */
static int
fgetLine(char **bufp, size_t *szp, int o, FILE *fp)
{
	char *buf = *bufp;
	size_t bufsz = *szp;
	struct stat fs;
	int x;

	if (fgets(&buf[o], (int)bufsz - o, fp) != NULL) {
	check_newline:
	x = o + (int)strlen(&buf[o]);
	if (buf[x - 1] == '\n')
		return x;
	/*
	 * We need to grow the buffer.
	 * The meta file can give us a clue.
	 */
	if (fstat(fileno(fp), &fs) == 0) {
		size_t newsz;
		char *p;

		newsz = ROUNDUP(((size_t)fs.st_size / 2), BUFSIZ);
		if (newsz <= bufsz)
		newsz = ROUNDUP((size_t)fs.st_size, BUFSIZ);
		if (newsz <= bufsz)
		return x;		/* truncated */
		DEBUG2(META, "growing buffer %u -> %u\n",
		   (unsigned)bufsz, (unsigned)newsz);
		p = bmake_realloc(buf, newsz);
		*bufp = buf = p;
		*szp = bufsz = newsz;
		/* fetch the rest */
		if (fgets(&buf[x], (int)bufsz - x, fp) == NULL)
		return x;		/* truncated! */
		goto check_newline;
	}
	}
	return 0;
}

static bool
prefix_match(const char *prefix, const char *path)
{
	size_t n = strlen(prefix);

	return strncmp(path, prefix, n) == 0;
}

static bool
has_any_prefix(const char *path, StringList *prefixes)
{
	StringListNode *ln;

	for (ln = prefixes->first; ln != NULL; ln = ln->next)
	if (prefix_match(ln->datum, path))
		return true;
	return false;
}

/* See if the path equals prefix or starts with "prefix/". */
static bool
path_starts_with(const char *path, const char *prefix)
{
	size_t n = strlen(prefix);

	if (strncmp(path, prefix, n) != 0)
	return false;
	return path[n] == '\0' || path[n] == '/' ||
		path[n] == '\\';
}

static bool
meta_ignore(GNode *gn, const char *p)
{
	char fname[MAXPATHLEN];

	if (p == NULL)
	return true;

	if (isAbs(*p)) {
	/* first try the raw path "as is" */
	if (has_any_prefix(p, &metaIgnorePaths)) {
#ifdef DEBUG_META_MODE
		DEBUG1(META, "meta_oodate: ignoring path: %s\n", p);
#endif
		return true;
	}
	cached_realpath(p, fname); /* clean it up */
	if (has_any_prefix(fname, &metaIgnorePaths)) {
#ifdef DEBUG_META_MODE
		DEBUG1(META, "meta_oodate: ignoring path: %s\n", p);
#endif
		return true;
	}
	}

	if (metaIgnorePatterns) {
	const char *expr;
	char *pm;

	/*
	 * XXX: This variable is set on a target GNode but is not one of
	 * the usual local variables.  It should be deleted afterwards.
	 * Ideally it would not be created in the first place, just like
	 * in a .for loop.
	 */
	Var_Set(gn, ".p.", p);
	expr = "${" MAKE_META_IGNORE_PATTERNS ":@m@${.p.:M$m}@}";
	pm = Var_Subst(expr, gn, VARE_WANTRES);
	/* TODO: handle errors */
	if (pm[0] != '\0') {
#ifdef DEBUG_META_MODE
		DEBUG1(META, "meta_oodate: ignoring pattern: %s\n", p);
#endif
		free(pm);
		return true;
	}
	free(pm);
	}

	if (metaIgnoreFilter) {
	char *fm;

	/* skip if filter result is empty */
	snprintf(fname, sizeof fname,
		 "${%s:L:${%s:ts:}}",
		 p, MAKE_META_IGNORE_FILTER);
	fm = Var_Subst(fname, gn, VARE_WANTRES);
	/* TODO: handle errors */
	if (*fm == '\0') {
#ifdef DEBUG_META_MODE
		DEBUG1(META, "meta_oodate: ignoring filtered: %s\n", p);
#endif
		free(fm);
		return true;
	}
	free(fm);
	}
	return false;
}

/*
 * When running with 'meta' functionality, a target can be out-of-date
 * if any of the references in its meta data file is more recent.
 * We have to track the latestdir on a per-process basis.
 */
#define LCWD_VNAME_FMT ".meta.%d.lcwd"
#define LDIR_VNAME_FMT ".meta.%d.ldir"

/*
 * It is possible that a .meta file is corrupted,
 * if we detect this we want to reproduce it.
 * Setting oodate true will have that effect.
 */
#define CHECK_VALID_META(p) if (!(p != NULL && *p != '\0')) { \
	warnx("%s: %u: malformed", fname, lineno); \
	oodate = true; \
	continue; \
	}

#define DEQUOTE(p) if (*p == '\'') {	\
	char *ep; \
	p++; \
	if ((ep = strchr(p, '\'')) != NULL) \
	*ep = '\0'; \
	}

static void
append_if_new(StringList *list, const char *str)
{
	StringListNode *ln;

	for (ln = list->first; ln != NULL; ln = ln->next)
	if (strcmp(ln->datum, str) == 0)
		return;
	Lst_Append(list, bmake_strdup(str));
}

/* A "reserved" variable to store the command to be filtered */
#define META_CMD_FILTER_VAR ".MAKE.cmd_filtered"

static char *
meta_filter_cmd(GNode *gn, char *s)
{
	Var_Set(gn, META_CMD_FILTER_VAR, s);
	s = Var_Subst(
	"${" META_CMD_FILTER_VAR ":${" MAKE_META_CMP_FILTER ":ts:}}",
	gn, VARE_WANTRES);
	return s;
}

static int
meta_cmd_cmp(GNode *gn, char *a, char *b, bool filter)
{
	int rc;

	rc = strcmp(a, b);
	if (rc == 0 || !filter)
	return rc;
	a = meta_filter_cmd(gn, a);
	b = meta_filter_cmd(gn, b);
	rc = strcmp(a, b);
	free(a);
	free(b);
	Var_Delete(gn, META_CMD_FILTER_VAR);
	return rc;
}

bool
meta_oodate(GNode *gn, bool oodate)
{
	static char *tmpdir = NULL;
	static char cwd[MAXPATHLEN];
	char lcwd[MAXPATHLEN];
	char latestdir[MAXPATHLEN];
	char fname[MAXPATHLEN];
	char fname3[MAXPATHLEN];
	FStr dname;
	const char *tname;
	char *p;
	char *link_src;
	char *move_target;
	static size_t cwdlen = 0;
	static size_t tmplen = 0;
	FILE *fp;
	bool needOODATE = false;
	StringList missingFiles;
	bool cmp_filter;

	if (oodate)
	return oodate;		/* we're done */

	dname = Var_Value(gn, ".OBJDIR");
	tname = GNode_VarTarget(gn);

	/* if this succeeds fname3 is realpath of dname */
	if (!meta_needed(gn, dname.str, fname3, false))
	goto oodate_out;
	dname.str = fname3;

	Lst_Init(&missingFiles);

	/*
	 * We need to check if the target is out-of-date. This includes
	 * checking if the expanded command has changed. This in turn
	 * requires that all variables are set in the same way that they
	 * would be if the target needs to be re-built.
	 */
	GNode_SetLocalVars(gn);

	meta_name(fname, sizeof fname, dname.str, tname, dname.str);

#ifdef DEBUG_META_MODE
	DEBUG1(META, "meta_oodate: %s\n", fname);
#endif

	if ((fp = fopen(fname, "r")) != NULL) {
	static char *buf = NULL;
	static size_t bufsz;
	unsigned lineno = 0;
	int x;
	StringListNode *cmdNode;

	if (buf == NULL) {
		bufsz = 8 * BUFSIZ;
		buf = bmake_malloc(bufsz);
	}

	if (cwdlen == 0) {
		if (getcwd(cwd, sizeof cwd) == NULL)
		err(1, "Could not get current working directory");
		cwdlen = strlen(cwd);
	}
	strlcpy(lcwd, cwd, sizeof lcwd);
	strlcpy(latestdir, cwd, sizeof latestdir);

	if (tmpdir == NULL) {
		tmpdir = getTmpdir();
		tmplen = strlen(tmpdir);
	}

	/* we want to track all the .meta we read */
	Global_Append(".MAKE.META.FILES", fname);

	cmp_filter = metaCmpFilter || Var_Exists(gn, MAKE_META_CMP_FILTER);

	cmdNode = gn->commands.first;
	while (!oodate && (x = fgetLine(&buf, &bufsz, 0, fp)) > 0) {
		lineno++;
		if (buf[x - 1] == '\n')
		buf[x - 1] = '\0';
		else {
		warnx("%s: %u: line truncated at %u", fname, lineno, x);
		oodate = true;
		break;
		}
		link_src = NULL;
		move_target = NULL;

		/* Delimit the record type. */
		p = buf;
#ifdef DEBUG_META_MODE
		DEBUG3(META, "%s: %u: %s\n", fname, lineno, buf);
#endif
		strsep(&p, " ");
		if (strcmp(buf, "CMD") == 0) {
		/*
		 * Compare the current command with the one in the
		 * meta data file.
		 */
		if (cmdNode == NULL) {
			DEBUG2(META, "%s: %u: there were more build commands in the meta data file than there are now...\n",
			   fname, lineno);
			oodate = true;
		} else {
			const char *cp;
			char *cmd = cmdNode->datum;
			bool hasOODATE = false;

			if (strstr(cmd, "$?") != NULL)
			hasOODATE = true;
			else if ((cp = strstr(cmd, ".OODATE")) != NULL) {
			/* check for $[{(].OODATE[:)}] */
			if (cp > cmd + 2 && cp[-2] == '$')
				hasOODATE = true;
			}
			if (hasOODATE) {
			needOODATE = true;
			DEBUG2(META, "%s: %u: cannot compare command using .OODATE\n",
				   fname, lineno);
			}
			cmd = Var_Subst(cmd, gn, VARE_UNDEFERR);
			/* TODO: handle errors */

			if ((cp = strchr(cmd, '\n')) != NULL) {
			int n;

			/*
			 * This command contains newlines, we need to
			 * fetch more from the .meta file before we
			 * attempt a comparison.
			 */
			/* first put the newline back at buf[x - 1] */
			buf[x - 1] = '\n';
			do {
				/* now fetch the next line */
				if ((n = fgetLine(&buf, &bufsz, x, fp)) <= 0)
				break;
				x = n;
				lineno++;
				if (buf[x - 1] != '\n') {
				warnx("%s: %u: line truncated at %u", fname, lineno, x);
				break;
				}
				cp = strchr(cp + 1, '\n');
			} while (cp != NULL);
			if (buf[x - 1] == '\n')
				buf[x - 1] = '\0';
			}
			if (p != NULL &&
			!hasOODATE &&
			!(gn->type & OP_NOMETA_CMP) &&
			(meta_cmd_cmp(gn, p, cmd, cmp_filter) != 0)) {
			DEBUG4(META, "%s: %u: a build command has changed\n%s\nvs\n%s\n",
				   fname, lineno, p, cmd);
			if (!metaIgnoreCMDs)
				oodate = true;
			}
			free(cmd);
			cmdNode = cmdNode->next;
		}
		} else if (strcmp(buf, "CWD") == 0) {
		/*
		 * Check if there are extra commands now
		 * that weren't in the meta data file.
		 */
		if (!oodate && cmdNode != NULL) {
			DEBUG2(META, "%s: %u: there are extra build commands now that weren't in the meta data file\n",
			   fname, lineno);
			oodate = true;
		}
		CHECK_VALID_META(p);
		if (strcmp(p, cwd) != 0) {
			DEBUG4(META, "%s: %u: the current working directory has changed from '%s' to '%s'\n",
			   fname, lineno, p, curdir);
			oodate = true;
		}
		}
	}

	fclose(fp);
	if (!Lst_IsEmpty(&missingFiles)) {
		DEBUG2(META, "%s: missing files: %s...\n",
		   fname, (char *)missingFiles.first->datum);
		oodate = true;
	}
	} else {
	if (writeMeta && (metaMissing || (gn->type & OP_META))) {
		const char *cp = NULL;

		/* if target is in .CURDIR we do not need a meta file */
		if (gn->path != NULL && (cp = lastSlash(gn->path)) != NULL) {
		if (strncmp(curdir, gn->path, (size_t)(cp - gn->path)) != 0) {
			cp = NULL;		/* not in .CURDIR */
		}
		}
		if (cp == NULL) {
		DEBUG1(META, "%s: required but missing\n", fname);
		oodate = true;
		needOODATE = true;	/* assume the worst */
		}
	}
	}

	Lst_DoneCall(&missingFiles, free);

	if (oodate && needOODATE) {
	/*
	 * Target uses .OODATE which is empty; or we wouldn't be here.
	 * We have decided it is oodate, so .OODATE needs to be set.
	 * All we can sanely do is set it to .ALLSRC.
	 */
	Var_Delete(gn, OODATE);
	Var_Set(gn, OODATE, GNode_VarAllsrc(gn));
	}

 oodate_out:
	FStr_Done(&dname);
	return oodate;
}

/* support for compat mode */

static HANDLE childPipe[2];

void meta_compat_start(void)
{
	if (CreatePipe(&childPipe[0], &childPipe[1], NULL, 0) == 0)
		Punt("failed to create pipe: %s", strerr(GetLastError()));
	if (SetNamedPipeHandleState(childPipe[0], &(DWORD){PIPE_NOWAIT},
		NULL, NULL) == 0)
		Punt("failed to set pipe handle state: %s", strerr(GetLastError()));
	if (SetHandleInformation(childPipe[1], HANDLE_FLAG_INHERIT,
		HANDLE_FLAG_INHERIT) == 0)
		Punt("failed to set pipe attributes: %s", strerr(GetLastError()));
}

HANDLE meta_compat_stdout(void)
{
	return childPipe[1];
}

void meta_compat_catch(char *cmd)
{
	char *buf;
	DWORD sz;

	if (PeekNamedPipe(childPipe[0], NULL, 0, NULL, &sz, NULL) == 0)
		Punt("failed to peek pipe: %s", strerr(GetLastError()));
	if (sz <= 0)
		return;

	{
		size_t len = strlen(cmd);
		buf = _alloca(len + 2);

		memcpy(buf, cmd, len);
		buf[len] = '\n';
		buf[len + 1] = '\0';

		cmd = buf;
	}
	buf = _alloca(sz + 1);

	if (ReadFile(childPipe[0], buf, sz, NULL, NULL) == 0)
		Punt("failed to read from pipe: %s", strerr(GetLastError()));

	fwrite(buf, 1, sz, stdout);
	fflush(stdout);

	buf[sz] = '\0';
	meta_job_output(NULL, cmd, "");
	meta_job_output(NULL, buf, "");

	CloseHandle(childPipe[0]);
	CloseHandle(childPipe[1]);
}

#endif /* USE_META */
