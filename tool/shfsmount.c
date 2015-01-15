/*
 *  This file is part of SHell File System.
 *  See http://shfs.sourceforge.net/ for more info.
 *
 *  Copyright (C) 2002-2004  Miroslav Spousta <qiq@ucw.cz>
 *
 *  Partialy inspired by smbmnt.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <mntent.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "shfsmount.h"
#include "proto.h"
#include "shfs_fs.h"

#define SHELL		"/bin/sh"
#define BUFFER_MAX	1024

/* remote root dir */
static char *root = NULL;

/* dir we are mounting to */
static char *mnt = NULL;

/* remote host user */
static char *user = NULL;

/* remote hostname */
static char *host = NULL;

/* full remote hostname (for mtab entry) */
static char *host_long = NULL;

/* remote port */
static char *port = NULL;

/* command to execute (as specified) */
static char *cmd_template = NULL;

/* command to execute */
static char *cmd = NULL;

/* user (& group) under the cmd is executed */
static uid_t cmd_uid = 0;
static gid_t cmd_gid = 0;
static int have_uid = 0;

/* do not update /etc/mtab */
static int nomtab = 0;

/* preserve owner of files */
static int preserve = 0;

/* stable symlinks */
static int stable = 0;

/* options for mount command */
static char options[BUFFER_MAX];

/* should be connection recovered after program exit? */
static int persistent = 0;

/* preferred type of connection */
static char *type = NULL;

/* should shfsmount print debug messages? */
int verbose = 0;

void
error(char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stderr, "shfsmount: ");
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	exit(2);
}

static void
usage(FILE *fp, int res)
{
	fprintf(fp, "Usage: shfsmount [OPTION] [[user@]host[:root]] mount_point\n"
	       "Mount remote file system using shell session.\n\n"
		"  -u, --cmd-user=USER\texecute command as this user (root only)\n"
		"  -c, --cmd=COMMAND\tcommand to connect to remote side (see below)\n"
		"  -P, --port=PORT\tconnect to this port\n"
		"  -p, --persistent\tmake connection persistent\n"
		"  -t, --type=TYPE\tconnection type (shell, perl)\n"
		"  -s, --stable\t\tderefference symbolic links (if possible)\n"
		"  -o, --options=OPTIONS\tmount options (see below)\n"
		"  -n, --nomtab\t\tdo not update /etc/mtab\n"
		"  -v, --verbose\t\tprint debug messages\n"
		"  -V, --version\t\tprint version number\n"
		"  -h, --help\t\tprint usage information\n"
		"  user\t\t\tlog in using this user name\n"
		"  host\t\t\tspecify remote host\n"
		"  root\t\t\tmake this directory root for mounted filesystem\n\n"
		"mount options (separated by comma):\n"
		"  cachesize=N\tread-ahead and write-back cache size in pages,\n"
		"  \t\tpage size is 4KB on i386; 0 = disable (default is 32)\n"
		"  cachemax=N\tmaximum number of cached files (default is 10)\n"
		"  preserve\tpreserve uid/gid (root only)\n"
		"  ttl=TIME\ttime to live (sec) for directory cache\n"
		"  uid=USER\towner of all files/dirs on mounted filesystem (root only)\n"
		"  gid=GROUP\tgroup of all files/dirs on mounted filesystem (root only)\n"
		"  rmode=MODE\troot dir mode (default is 700)\n"
		"  suid, dev\tsee mount(8) (root only)\n"
		"  ro, rw, nosuid, nodev, exec, noexec, user, users: see mount(8)\n"
		"  cmd-user, cmd, port, persistent, type, stable: see above\n\n"
		"%%u %%h %%P in COMMAND are substituted by user, host and port respectively.\n"
		"Report bugs to: qiq@ucw.cz\n");
	exit(res);
}

static void
version(void)
{
	fprintf(stderr, "shfsmount version %s(%d)\n", SHFS_VERSION, PROTO_VERSION);
	exit(0);
}

static char *
strnconcat(char *str, int n, ...)
{
	va_list args;
	char *s;

	va_start(args, n);
	while ((s = va_arg(args, char *)))
		strncat(str, s, n);
	va_end(args);

	return str;
}

static char *
fullpath(char *path)
{
        char full[MAXPATHLEN];

	if (strlen(path) > MAXPATHLEN-1)
		return NULL;
        if (!realpath(path, full))
		return NULL;
	return strdup(full);
}

#define GROUPS_MAX 1024

static int
mount_ok(char *dir)
{
	struct stat st;
	int groups[GROUPS_MAX];
	int i, n;

	if (chdir(dir) != 0)
		return 0;
        if (lstat(".", &st) != 0)
		return 0;
        if (!S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                return 0;
        }
	if (getuid() == 0)
		return 1;
	if (getuid() == st.st_uid) {
		if ((st.st_mode & S_IRWXU) == S_IRWXU)
			return 1;
		errno = EPERM;
		return 0;
	}
	groups[0] = getgid();
	n = getgroups(GROUPS_MAX-1, groups+1);
	for (i = 0; i <= n; i++) {
		if (st.st_gid == groups[i]) {
			if ((st.st_mode & S_IRWXG) == S_IRWXG)
				return 1;
		}
	}
	errno = EPERM;
        return 0;
}

static int
update_mtab(char *mnt, char *dev, char *opts)
{
	int fd;
	FILE *mtab;
	struct mntent ment;

        ment.mnt_fsname = dev;
        ment.mnt_dir = mnt;
        ment.mnt_type = "shfs";
        ment.mnt_opts = opts;
        ment.mnt_freq = 0;
        ment.mnt_passno = 0;

        if ((fd = open(MOUNTED"~", O_RDWR|O_CREAT|O_EXCL, 0600)) == -1) {
                fprintf(stderr, "Can't get "MOUNTED"~ lock file");
                return 0;
        }
        close(fd);
        if ((mtab = setmntent(MOUNTED, "a+")) == NULL) {
                fprintf(stderr, "Can't open " MOUNTED);
                return 0;
        }
        if (addmntent(mtab, &ment) == 1) {
                fprintf(stderr, "Can't write mount entry");
                return 0;
        }
        if (fchmod(fileno(mtab), 0644) == -1) {
                fprintf(stderr, "Can't set perms on "MOUNTED);
                return 0;
        }
        endmntent(mtab);
        if (unlink(MOUNTED"~") == -1) {
                fprintf(stderr, "Can't remove "MOUNTED"~");
                return 0;
        }
	return 1;
}


static uid_t
get_uid(char *s, gid_t *gid)
{
	struct passwd *passwd;
	char *e;
	long id = strtol(s, &e, 10);

	if (*s && *e == '\0')
		passwd = getpwuid(id);
	else
		passwd = getpwnam(s);
	if (!passwd)
		error("Unknown user: %s", s);

	if (gid)
		*gid = passwd->pw_gid;
	
	return passwd->pw_uid;
}

static gid_t
get_gid(char *s)
{
	struct group *group;
	char *e;
	long id = strtol(s, &e, 10);

	if (*s && *e == '\0')
		group = getgrgid(id);
	else
		group = getgrnam(s);
	if (!group) {
		fprintf(stderr, "Unknown group: %s", s);
		exit(1);
	}
	
	return group->gr_gid;
}

static int
create_socket_sh(void)
{
	pid_t child;
	int fd[2], null;
	char *execv[] = { "sh", "-c", NULL, NULL };

	/* ensure fd 0-2 are open */
	do {
		if ((null = open("/dev/null", 0)) < 0)
			error("open(/dev/null): %s", strerror(errno));
	} while (null < 3);
	close(null);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == -1)
		error("socketpair: %s", strerror(errno));

	switch ((child = fork())) {
	case 0:			 /* child */
		if ((dup2(fd[0], STDIN_FILENO) == -1) || (dup2(fd[0], STDOUT_FILENO) == -1))
			error("dup2: %s", strerror(errno));
		close(fd[0]);
		close(fd[1]);

		null = open("/dev/tty", O_WRONLY);
		if (null >= 0) {
			/* stderr is /dev/tty (because of autofs) */
			if (dup2(null, STDERR_FILENO) == -1)
				error("dup2: %s", strerror(errno));
			close(null);
		}

		if (have_uid) {
			if (setgid(cmd_gid))
				error("setgid: %s", strerror(errno));
		} else {
			cmd_uid = getuid();
		}
		if (setuid(cmd_uid))
			error("setuid: %s", strerror(errno));

		execv[2] = cmd;
		execvp("/bin/sh", execv);
		error("exec: %s: %s", execv[0], strerror(errno));
	case -1:
		error("fork: %s", strerror(errno));
	}

	close(fd[0]);
	close(null);

	if (!init_sh(fd[1], type, root, stable, preserve)) {
		close(fd[1]);
		return -1;
	}

	return fd[1];
}

static void
wait_on_socket_sh(int fd)
{
	int status;

	close(fd);
	if (wait(&status) < 0)
		error("wait: %s", strerror(errno));
}

static int
create_socket(void)
{
	/* possible sftp/ssh extension here */
	return create_socket_sh();
}

#define MAX_SLEEP 300
#define MIN_SLEEP 5

/* exponential slowdown upto five mins */
static void
wait_on_socket(int fd)
{
	static int sleeptime = MIN_SLEEP;
	static time_t last = 0;
	int new_sleeptime;
	time_t now;

	new_sleeptime = sleeptime * 2 < MAX_SLEEP ? sleeptime * 2 : MAX_SLEEP;
	
	now = time(NULL);
	if (now < last+sleeptime) {
		VERBOSE("sleeping %lds\n", last+sleeptime - now);
		sleep(last+sleeptime - now);
		sleeptime = new_sleeptime;
	} else {
		sleeptime = MIN_SLEEP;
	}
	last = time(NULL);
	
	wait_on_socket_sh(fd);
}

static void
daemonize(void)
{
	pid_t pid = fork();

	if (pid == -1)
		error("fork: %s", strerror(errno));
	if (pid != 0)
		exit(0);
	setsid();
}

static struct option long_options[] = {
	{"cmd-user", 1, NULL, 'u'},
	{"cmd", 1, NULL, 'c'},
	{"port", 1, NULL, 'P'},
	{"persistent", 0, NULL, 'p'},
	{"nomtab", 0, NULL, 'n'},
	{"type", 1, NULL, 't'},
	{"stable", 1, NULL, 's'},
	{"options", 1, NULL, 'o'},
	{"verbose", 0, NULL, 'v'},
	{"version", 0, NULL, 'V'},
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

int
main(int argc, char **argv)
{
	char buf[BUFFER_MAX];
	char *s, *c, *tmp;
	int res, sock;

	snprintf(options, sizeof(options), "version=%d", PROTO_VERSION);
	while(1) {
		int c;
		int index = 0;
		char *r;

		c = getopt_long(argc, argv, "u:c:P:pnt:so:vVhrO:", long_options, &index);
		if (c == -1)
			break;

		switch (c) {
		case 'u':
			if (getuid() != 0)
				error("Only root can do this!");
			cmd_uid = get_uid(optarg, &cmd_gid);
			have_uid = 1;
			break;
		case 'c':
			cmd_template = optarg;
			break;
		case 'P':
			if (strtol(optarg, &s, 10) == 0 || *s)
				error("Invalid port: %s", optarg);
			port = optarg;
			break;
		case 'p':
			persistent = 1;
			break;
		case 'n':
			nomtab = 1;
			break;
		case 't':
			type = optarg;
			break;
		case 's':
			stable = 1;
		case 'o':
			tmp = optarg;
			while ((s = strsep(&tmp, ","))) {
				/* check for root-only options */
				if ((getuid() != 0) && !(strcmp(s, "preserve")
				   && strncmp(s, "cmd-user=", 9)
				   && strncmp(s, "uid=", 4) && strncmp(s, "gid=", 4) 
				   && strcmp(s, "suid") && strcmp(s, "dev")))
					error("'%s' used by non-root user!", s);

				if (!strncmp(s, "cmd-user=", 9)) {
					if (getuid() != 0)
						error("Only root can do this!");
					cmd_uid = get_uid(s+9, &cmd_gid);
					have_uid = 1;
				} else if (!strncmp(s, "cmd=", 4)) {
					cmd_template = s+4;
				} else if (!strncmp(s, "port=", 5)) {
					if (strtol(s+5, &r, 10) == 0 || *r)
						error("Invalid port: %s", s+5);
					port = s+5;
				} else if (!strncmp(s, "persistent", 10)) {
					persistent = 1;
				} else if (!strncmp(s, "preserve", 8)) {
					preserve = 1;
					strnconcat(options, sizeof(options), ",", "preserve", NULL);
				} else if (!strncmp(s, "type=", 5)) {
					type = s+5;
				} else if (!strncmp(s, "stable", 6)) {
					stable = 1;
				} else if (!strncmp(s, "uid=", 4)) {
					snprintf(buf, sizeof(buf), "uid=%u", get_uid(s+4, NULL));
					strnconcat(options, sizeof(options), ",", buf, NULL);
				} else if (!strncmp(s, "gid=", 4)) {
					snprintf(buf, sizeof(buf), "gid=%u", get_gid(s+4));
					strnconcat(options, sizeof(options), ",", buf, NULL);
				} else if (!strcmp(s, "user")) {
					struct passwd *pw = getpwuid(getuid());
					if (pw) {
						snprintf(buf, sizeof(buf), "user=%s", pw->pw_name);
						strnconcat(options, sizeof(options), ",", buf, NULL);
					}
				} else {
					strnconcat(options, sizeof(options), ",", s, NULL);
				}
			}
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			version();
			break;
		case 'h':
			usage(stdout, 0);
			break;
		case 'O':
		case 'r':
			/* backward compatibility */
			break;
		case '?':
		case ':':
			exit(2);
			break;
		default:
			usage(stderr, 1);
			break;
		}
	}

	/* get host and mount point */
	if (!(argc - optind > 0))
		usage(stderr, 1);
	if (argc - optind == 1) {
		mnt = argv[optind];
	} else {
		host = argv[optind];
		mnt = argv[optind+1];
	}

	if (host) {
		host_long = strdup(host);
	        if ((c = strchr(host, '@'))) {
	                *c = '\0';
			user = host;
	                host = c + 1;
		}

	       /*
		* stuart at gnqs.org
		* Updated 1st June 2003
		*
		* the colon is now used as the separator
		* between the hostname and the root dir
		*
		* this makes it work more like scp(1)
		*/
	
		if ((c = strchr(host, ':'))) {
	                *c = '\0';
			root = c + 1;
		} else if (*host == '/') {
			root = host;
			host = NULL;
		}
	}
	mnt = fullpath(mnt);
	if (!mnt)
		error("Invalid path: %s", argv[optind+1]);
	if (!mount_ok(mnt))
		error("%s: %s", mnt, strerror(errno));
	strnconcat(options, sizeof(options), ",mnt=", mnt, NULL);
	if (verbose) {
		snprintf(buf, sizeof(buf), ",debug=%u", verbose);
		strnconcat(options, sizeof(options), buf, NULL);
	}

	/* setup cmd */
	cmd = malloc(BUFFER_MAX);
	if (!cmd)
		error("Insufficient memory");
	if (cmd_template) {
		tmp = cmd_template;
		*cmd = '\0';
		while ((s = strsep(&tmp, "%"))) {
			strnconcat(cmd, BUFFER_MAX, s, NULL);
			if (tmp) {
				switch (*tmp) {
				case 'u':
					if (user)
						strnconcat(cmd, BUFFER_MAX, user, NULL);
					tmp++;
					break;
				case 'h':
					if (host)
						strnconcat(cmd, BUFFER_MAX, host, NULL);
					tmp++;
					break;
				case 'P':
					if (port)
						strnconcat(cmd, BUFFER_MAX, port, NULL);
					tmp++;
					break;
				default:
					strnconcat(cmd, BUFFER_MAX, "%", NULL);
					break;
				}
			}
		}
	} else {
		if (!host)
			error("Unknown remote host");
		snprintf(cmd, BUFFER_MAX, "exec ssh %s%s %s%s %s %s", 
			 port ? "-p " : "", port ? port : "",
			 user ? "-l " : "", user ? user : "",
			 host, SHELL);
	}

	VERBOSE("cmd: %s, options: \"%s\"\n", cmd, options);
	VERBOSE("user: %s, host: %s, root: %s, mnt: %s, port: %s, cmd-user: %d\n", user, host, root, mnt, port, cmd_uid);

	if (persistent)
		daemonize();

	sock = create_socket();
	if (sock < 0)
		error("Cannot create connection");

	snprintf(buf, sizeof(buf), ",fd=%d", sock);
	strnconcat(options, sizeof(options), buf, NULL);

	if (mount("none", mnt, "shfs", 0, options) < 0) {
		if (errno == ENODEV)
			error("shfs filesystem not supported by the kernel");
		else
			error(strerror(errno));
	}
        if (!nomtab) {
		res = !update_mtab(mnt, host_long ? host_long : "(none)", options);
	} else {
		res = 0;
	}
	free(host_long);

	if (persistent) {
		while (1) {
			int fdmnt;

			wait_on_socket(sock);
			sock = create_socket();
		
			/* pass new socket to the kernel */
			fdmnt = open(mnt, 0);
			if (fdmnt == -1)
				error("open: %s", strerror(errno));
			if (ioctl(fdmnt, SHFS_IOC_NEWCONN, sock) != 0)
				exit(0);
			close(fdmnt);
		}
	} else {
		close(sock);
	}

	free(mnt);
	exit(res);
}

