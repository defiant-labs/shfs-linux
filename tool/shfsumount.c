/* partialy inspired by smbumount.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <sys/utsname.h>

#include "shfs_fs.h"

static void usage(void)
{
	fprintf(stderr, "Usage: shfsumount mount_point\n"
			"Unmount shares previously mounted by shfsmount.\n\n"
			"Report bugs to: qiq@ucw.cz\n");
	exit(1);
}

static char *fullpath(char *path)
{
        char full[MAXPATHLEN];

	if (strlen(path) > MAXPATHLEN-1)
		return NULL;
        if (!realpath(path, full))
		return NULL;
	return strdup(full);
}

static int umount_ok(char *dir)
{
	struct stat st;
	struct statfs stfs;

        if (statfs(dir, &stfs) != 0)
		return 0;
	if (stfs.f_type != SHFS_SUPER_MAGIC) {
		errno = EINVAL;
		return 0;
	}
        if (lstat(dir, &st) != 0)
		return 0;
        if (!S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                return 0;
        }
        if ((getuid() != 0) && ((getuid() != st.st_uid) || 
	     ((st.st_mode & S_IRWXU) != S_IRWXU))) {
                errno = EPERM;
                return 0;
        }
        return 1;
}

static int update_mtab(char *mnt)
{
	int fd;
	FILE *mtab;
	FILE *new_mtab;
	struct mntent *ment;

        if ((fd = open(MOUNTED"~", O_RDWR|O_CREAT|O_EXCL, 0600)) == -1) {
                fprintf(stderr, "Can't get "MOUNTED"~ lock file");
                return 0;
        }
        close(fd);
        if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
                fprintf(stderr, "Can't open " MOUNTED ": %s\n", strerror(errno));
                return 0;
        }
        if ((new_mtab = setmntent(MOUNTED".tmp", "w")) == NULL) {
                fprintf(stderr, "Can't open " MOUNTED".tmp" ": %s\n", strerror(errno));
                endmntent(mtab);
                return 0;
        }
        while ((ment = getmntent(mtab))) {
                if (strcmp(ment->mnt_dir, mnt) != 0)
                        addmntent(new_mtab, ment);
        }
        endmntent(mtab);
        if (fchmod(fileno(new_mtab), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0) {
                fprintf(stderr, "Error changing mode of %s: %s\n",
                        MOUNTED".tmp", strerror(errno));
                return 0;
        }
        endmntent(new_mtab);
        if (rename(MOUNTED".tmp", MOUNTED) < 0) {
                fprintf(stderr, "Cannot rename %s to %s: %s\n",
                        MOUNTED".tmp", MOUNTED, strerror(errno));
                return 0;
        }
        if (unlink(MOUNTED"~") == -1) {
                fprintf(stderr, "Can't remove "MOUNTED"~");
                return 0;
        }

	return 1;
}

int main(int argc, char **argv)
{
	char *mnt;

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "-v")) {
		fprintf(stderr, "shfsumount version %s(%d)\n", SHFS_VERSION, PROTO_VERSION);
		exit(0);
	}
	
	mnt = fullpath(argv[1]);
	if (!mnt) {
		fprintf(stderr, "%s: Invalid path\n", argv[1]);
		exit(1);
	}
	
	if (!umount_ok(mnt)) {
		fprintf(stderr, "%s: %s\n", mnt, strerror(errno));
		exit(1);
	}
	
	if (umount(mnt) == -1) {
		perror(argv[0]);
		exit(1);
	}

	exit(!update_mtab(mnt));
}
