#ifndef _PROC_H
#define _PROC_H

#include "shfs_fs_sb.h"

static inline int
sock_lock(struct shfs_sb_info *info)
{
	int result;
	DEBUG("?\n");

	//result = down_interruptible(&(info->sock_sem));
	result = mutex_lock_interruptible(&(info->shfs_mutex));
	//result = mutex_lock_interruptible(&info->shfs_mutex);

	DEBUG("!\n");
	return (result != -EINTR);
}

static inline void
sock_unlock(struct shfs_sb_info *info)
{
	//up(&(info->sock_sem));
	mutex_unlock(&(info->shfs_mutex));
	//mutex_unlock(&info->shfs_mutex));
	DEBUG("\n");
}

#endif	/* _PROC_H */