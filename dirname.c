/*	$NetBSD: dirname.c,v 1.14 2018/09/27 00:45:34 kre Exp $	*/

/*-
 * Copyright (c) 1997, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein and Jason R. Thorpe.
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

#include "make.h"

#define MIN(a, b) ((a < b) ? a : b)

static size_t
xdirname_r(const char *path, char *buf, size_t buflen)
{
	const char *endp;
	size_t len;
	char abs[3] = { '\0', ':', '\\' };

	if (path == NULL || *path == '\0') {
		path = ".";
		len = 1;
		goto out;
	}

	/* Strip trailing slashes, if any. */
	endp = path + strlen(path) - 1;
	while (endp != path && (*endp == '/' ||
		*endp == '\\'))
		endp--;

	/* Find the start of the dir */
	while (endp > path && *endp != '/' &&
		*endp != '\\')
		endp--;

	if (path == endp) {
		if (path[1] == ':') {
			abs[0] = *path;
			path = abs;
			len = 3;
			goto out;
		}

		path = ".";
		len = 1;
		goto out;
	}
	else if (path == endp - 2 &&
		path[1] == ':') {
		abs[0] = *path;
		path = abs;
		len = 3;
		goto out;
	}

	do
	{
		endp--;
	} while (endp > path && (*endp == '/' ||
		*endp == '\\'));

	len = endp - path + 1;
out:
	if (buf != NULL && buflen != 0) {
		buflen = MIN(len, buflen - 1);
		if (buf != path)
			memcpy(buf, path, buflen);
		buf[buflen] = '\0';
	}
	return len;
}

char *
dirname(char *path)
{
	static char result[PATH_MAX];
	(void)xdirname_r(path, result, sizeof(result));
	return result;
}
