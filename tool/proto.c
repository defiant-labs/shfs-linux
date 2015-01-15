/*
 *  This file is part of SHell File System.
 *  See http://shfs.sourceforge.net/ for more info.
 *
 *  Copyright (C) 2002-2004  Miroslav Spousta <qiq@ucw.cz>
 *
 *  Protocol initialization.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "shfsmount.h"

struct proto {
	char *id;
	const char *test;
	char *code;
};

static char perl_test[] =
#include  "perl-test.h"
"\n";

static char perl_code[] =
"exec perl -e '"
#include "perl-code.h"
"'\n";

/* test we have basic commands; credits: */
static char shell_test[] = 
#include "shell-test.h"
"\n";

static char shell_code[] = 
#include "shell-code.h"
"\n";

struct proto sh[] = {
	{ "perl", perl_test, perl_code },
	{ "shell", shell_test, shell_code },
	{ NULL, NULL, NULL },
};

static ssize_t
readln(int fd, void *data, size_t n)
{
	size_t rd, result;

	rd = 0;
	while (n > rd) {
		result = read(fd, data + rd, n - rd);
		if (result == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return rd;
		}
		if (result == 0)
			return rd;
		if (memchr(data + rd, '\n', n - rd))
			return rd + result;
		rd += result;
	}
	return rd;
}

static ssize_t
writeall(int fd, const void *data, size_t n)
{
	size_t wr, res;

	wr = 0;
	while (n > wr) {
		res = write(fd, data + wr, n - wr);
		if (res == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return wr;
		}
		if (res == 0)
			return wr;
		wr += res;
	}
	return wr;
}

int
init_sh(int fd, const char *desired, const char *root, 
	int stable, int preserve)
{
	char buffer[BUFFER_MAX];
	struct proto *proto;
	int rd;

	for (proto = sh; proto->id; proto++) {
		char *r, *s = buffer;
		int rok = 0, rstable = 0, rpreserve = 0;

		if (desired && strcmp(proto->id, desired))
			continue;

		VERBOSE("Testing %s... ", proto->id);
		writeall(fd, proto->test, strlen(proto->test));
		do {
			rd = readln(fd, &buffer, sizeof(buffer)-1);
			if (rd < 0)
				return 0;
			buffer[rd] = '\0';
			if (rd > 0 && buffer[rd-1] == '\n')
				buffer[rd-1] = '\0';
			VERBOSE("%s\n", buffer);
		} while (rd > 0 && !strstr(buffer, "ok") && !strstr(buffer, "failed"));

		if (!strlen(buffer))
			return 0;

		s = buffer;
		while ((r = strsep(&s, " "))) {
			if (!strcmp(r, "ok"))
				rok = 1;
			else if (!strcmp(r, "stable"))
				rstable = 1;
			else if (!strcmp(r, "preserve"))
				rpreserve = 1;
			else if (strcmp(r, "failed"))
				fprintf(stderr, "Warning: unknown capability (%s): %s\n", proto->id, r);
		}
		
		if (rok) {
			if (stable && !rstable) {
				if (!desired)
					continue;
				error("%s: stable not supported.", proto->id);
			}
			if (preserve && !rpreserve) {
				if (!desired)
					continue;
				error("%s: preserve not supported", proto->id);
			}
			break;
		}
	}

	if (!proto->id)
		return 0;

	VERBOSE("selected: %s\n", proto->id);
	writeall(fd, proto->code, strlen(proto->code));
	rd = readln(fd, buffer, sizeof(buffer)-1);
	if (rd < 0) {
		VERBOSE("%s\n", strerror(rd));
		return 0;
	}
	buffer[rd] = '\0';
	DEBUG("reply: %s", buffer);
	if (strcmp(buffer, "### 200\n"))
		return 0;
	snprintf(buffer, sizeof(buffer), "s_init '%s'%s%s\n", 
		 root ? root : "",
		 stable ? " stable" : "",
		 preserve ? " preserve" : "");
	writeall(fd, buffer, strlen(buffer));
	rd = readln(fd, &buffer, sizeof(buffer)-1);
	if (rd < 0) {
		VERBOSE("%s\n", strerror(rd));
		return 0;
	}
	buffer[rd] = '\0';
	DEBUG("reply: %s", buffer);
	if (strcmp(buffer, "### 200\n"))
		return 0;

	return 1;
}
