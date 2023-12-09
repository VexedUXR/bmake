/*	$NetBSD: util.c,v 1.78 2021/12/15 12:58:01 rillig Exp $	*/

/*
 * Missing stuff from OS's
 *
 *	$Id: util.c,v 1.50 2021/12/21 18:47:24 sjg Exp $
 */

#include <errno.h>
#include <time.h>

#include "make.h"

int
unsetenv(const char *name)
{
	if (getenv(name) == NULL)
		return 0;

	size_t len = strlen(name);
	char *envstring = _alloca(len + 2);

	memcpy(envstring, name, len);
	envstring[len] = '=';
	envstring[len + 1] = '\0';

	return _putenv(envstring);
}

int
setenv(const char *name, const char *value, int rewrite)
{
	if (getenv(name) != NULL && !rewrite)
		return 0;

	size_t len1 = strlen(name);
	size_t len2 = strlen(value);
	char *envstring = _alloca(len1 + len2 + 2);

	memcpy(envstring, name, len1);
	if (*value == '=')
		memcpy(envstring + len1, value, len2 + 1);
	else {
		envstring[len1] = '=';
		memcpy(envstring + len1 + 1, value, len2 + 1);
	}

	return _putenv(envstring);
}

uint32_t
random(void)
{
	return (uint32_t)rand()<<16|(uint32_t)rand();
}

static void
vwarnx(MAKE_ATTR_PRINTFLIKE const char *fmt, 
	va_list args)
{
	fprintf(stderr, "%s: ", progname);
	if ((fmt)) {
		vfprintf(stderr, fmt, args);
		fprintf(stderr, ": ");
	}
}

static void
vwarn(MAKE_ATTR_PRINTFLIKE const char *fmt,
	va_list args)
{
	vwarnx(fmt, args);
	fprintf(stderr, "%s\n", strerror(errno));
}

static void
verr(int eval, MAKE_ATTR_PRINTFLIKE const char *fmt,
	va_list args)
{
	vwarn(fmt, args);
	exit(eval);
}

static void
verrx(int eval, MAKE_ATTR_PRINTFLIKE const char *fmt,
	va_list args)
{
	vwarnx(fmt, args);
	exit(eval);
}

void
err(int eval, MAKE_ATTR_PRINTFLIKE const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(eval, fmt, ap);
	va_end(ap);
}

void
errx(int eval, MAKE_ATTR_PRINTFLIKE const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(eval, fmt, ap);
	va_end(ap);
}

void
warn(MAKE_ATTR_PRINTFLIKE const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void
warnx(MAKE_ATTR_PRINTFLIKE const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}
