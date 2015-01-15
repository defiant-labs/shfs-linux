#ifndef _SHFS_FS_SB_H
#define _SHFS_FS_SB_H

#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/types.h>

#ifdef __KERNEL__

struct shfs_fileops {
	int (*readdir)(struct shfs_sb_info *info, char *dir, struct file *filp, void *dirent, filldir_t filldir, struct shfs_cache_control *ctl);
	int (*stat)(struct shfs_sb_info *info, char *file, struct shfs_fattr *fattr);
	int (*open)(struct shfs_sb_info *info, char *file, int mode);
	int (*read)(struct shfs_sb_info *info, char *file, unsigned offset,
		    unsigned count, char *buffer, unsigned long ino);
	int (*write)(struct shfs_sb_info *info, char *file, unsigned offset,
		     unsigned count, char *buffer, unsigned long ino);
	int (*mkdir)(struct shfs_sb_info *info, char *dir);
	int (*rmdir)(struct shfs_sb_info *info, char *dir);
	int (*rename)(struct shfs_sb_info *info, char *old, char *new);
	int (*unlink)(struct shfs_sb_info *info, char *file);
	int (*create)(struct shfs_sb_info *info, char *file, int mode);
	int (*link)(struct shfs_sb_info *info, char *old, char *new);
	int (*symlink)(struct shfs_sb_info *info, char *old, char *new);
	int (*readlink)(struct shfs_sb_info *info, char *name, char *real_name);
	int (*chmod)(struct shfs_sb_info *info, char *file, umode_t mode);
	int (*chown)(struct shfs_sb_info *info, char *file, uid_t user);
	int (*chgrp)(struct shfs_sb_info *info, char *file, gid_t group);
	int (*trunc)(struct shfs_sb_info *info, char *file, loff_t size);
	int (*settime)(struct shfs_sb_info *info, char *file, int atime, int mtime, struct timespec *time);
	int (*statfs)(struct shfs_sb_info *info, struct kstatfs *attr);
	int (*finish)(struct shfs_sb_info *info);
};

#define info_from_inode(inode) ((struct shfs_sb_info *)(inode)->i_sb->s_fs_info)
#define info_from_dentry(dentry) ((struct shfs_sb_info *)(dentry)->d_sb->s_fs_info)
#define info_from_sb(sb) ((struct shfs_sb_info *)(sb)->s_fs_info)

struct shfs_sb_info {
	struct mutex shfs_mutex;	/* global shfs lock */
	struct shfs_fileops fops;
	int version;
	int ttl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	kuid_t uid;
	kgid_t gid;
#else
	uid_t uid;
	gid_t gid;
#endif
	mode_t root_mode;
	mode_t fmask;
	char mount_point[SHFS_PATH_MAX];
	//struct semaphore sock_sem;	/* next 4 vars are guarded */
	struct file *sock;
	char *sockbuf;
	char *readlnbuf;
	int readlnbuf_len;
	spinlock_t fcache_lock;		/* fcache_free is guarded */
	int fcache_free;
	int fcache_size; 
	int garbage_read;
	int garbage_write;
	int garbage:1;
	int readonly:1;
	int preserve_own:1;
	int stable_symlinks:1;
};

#endif /* __KERNEL__ */

#endif /* _SHFS_FS_SB_H */
