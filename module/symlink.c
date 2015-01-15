/*
 * symlink.c
 *
 * Symlink resolving implementation.
 */

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>

#include "shfs_fs.h"
#include "shfs_fs_sb.h"
#include "shfs_debug.h"
#include "proc.h"

static int
shfs_readlink(struct dentry *dentry, char *buffer, int bufflen)
{
	struct shfs_sb_info *info = info_from_dentry(dentry);
	char name[SHFS_PATH_MAX];
	char real_name[SHFS_PATH_MAX];
	int result;
	
	DEBUG("%s\n", dentry->d_name.name);

	result = -ENAMETOOLONG;
	if (get_name(dentry, name) < 0)
		goto error;

	result = info->fops.readlink(info, name, real_name);
	if (result < 0)
		goto error;
	DEBUG("%s\n", real_name);
	result = vfs_readlink(dentry, buffer, bufflen, real_name);
error:
	return result;
}

static void *
shfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct shfs_sb_info *info = info_from_dentry(dentry);
	char name[SHFS_PATH_MAX];
	char real_name[SHFS_PATH_MAX];
	
	DEBUG("%s\n", dentry->d_name.name);

	if (get_name(dentry, name) < 0)
		goto error;

	if (!info->fops.readlink(info, name, real_name))
		goto error;

	DEBUG("%s\n", real_name);
	vfs_follow_link(nd, real_name);
error:
	return NULL;
}

struct inode_operations shfs_symlink_inode_operations = {
	.readlink		= shfs_readlink,
	.follow_link	= shfs_follow_link,
	.getattr		= shfs_getattr,
	.setattr		= shfs_notify_change,
};
