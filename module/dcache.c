/*
 * cache.c
 *
 * Copyright (C) 1997 by Bill Hawes
 * Copyright (C) 2004 Miroslav Spousta
 *
 * Routines to support directory cacheing using the page cache.
 * This cache code is almost directly taken from smbfs/ncpfs.
 *
 */

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/dirent.h>
#include <linux/pagemap.h>
#include <asm/page.h>

#include "shfs_fs.h"
#include "shfs_fs_sb.h"
#include "shfs_debug.h"

/*
 * Force the next attempt to use the cache to be a timeout.
 * If we can't find the page that's fine, it will cause a refresh.
 */
void
shfs_invalid_dir_cache(struct inode * dir)
{
	struct shfs_sb_info *info = info_from_inode(dir);
	union  shfs_dir_cache *cache = NULL;
	struct page *page = NULL;

	page = grab_cache_page(&dir->i_data, 0);
	if (!page)
		goto out;

	if (!PageUptodate(page))
		goto out_unlock;

	cache = kmap(page);
	cache->head.time = jiffies - SHFS_MAX_AGE(info);

	kunmap(page);
	SetPageUptodate(page);
out_unlock:
	unlock_page(page);
	page_cache_release(page);
out:
	return;
}

/*
 * Mark all dentries for 'parent' as invalid, forcing them to be re-read
 */
void
shfs_invalidate_dircache_entries(struct dentry *parent)
{
	struct shfs_sb_info *info = info_from_dentry(parent);
	struct list_head *next;
	struct dentry *dentry;

	//spin_lock(&dcache_lock);
	mutex_lock(&info->shfs_mutex);
	next = parent->d_subdirs.next;
	while (next != &parent->d_subdirs) {
		//dentry = list_entry(next, struct dentry, d_child);
		dentry = list_entry(next, struct dentry, d_u.d_child);
		dentry->d_fsdata = NULL;
		shfs_age_dentry(info, dentry);
		next = next->next;
	}
	//spin_unlock(&dcache_lock);
	mutex_unlock(&info->shfs_mutex);
}

/*
 * dget, but require that fpos and parent matches what the dentry contains.
 * dentry is not known to be a valid pointer at entry.
 */
struct dentry *
shfs_dget_fpos(struct dentry *dentry, struct dentry *parent, unsigned long fpos)
{
	struct dentry *dent = dentry;
	struct list_head *next;

	if (d_validate(dent, parent)) {
		if ((unsigned long)dent->d_fsdata == fpos) {
			if (!dent->d_inode) {
				dput(dent);
				dent = NULL;
			}
			return dent;
		}
		dput(dent);
	}

	/* If a pointer is invalid, we search the dentry. */
	//spin_lock(&dcache_lock);
	next = parent->d_subdirs.next;
	while (next != &parent->d_subdirs) {
		//dent = list_entry(next, struct dentry, d_child);
		dent = list_entry(next, struct dentry, d_u.d_child);
		if ((unsigned long)dent->d_fsdata == fpos) {
			/*if (dent->d_inode)
				dget_locked(dent);
			else*/
				dent = NULL;
			goto out_unlock;
		}
		next = next->next;
	}
	dent = NULL;
out_unlock:
	//spin_unlock(&dcache_lock);
	return dent;
}

/*
 * Create dentry/inode for this file and add it to the dircache.
 */
int
shfs_fill_cache(struct file *filp, void *dirent, filldir_t filldir,
	       struct shfs_cache_control *ctrl, struct qstr *qname,
	       struct shfs_fattr *entry)
{
	struct dentry *newdent, *dentry = filp->f_dentry;
	struct inode *newino, *inode = dentry->d_inode;
	struct shfs_cache_control ctl = *ctrl;
	int valid = 0;
	int hashed = 0;
	ino_t ino = 0;

//	struct inode *lower_inode = get_lower_inode(dentry);

	qname->hash = full_name_hash(qname->name, qname->len);

	if (dentry->d_op && dentry->d_op->d_hash)
//		if (dentry->d_op->d_hash(dentry, lower_inode, qname) != 0)
			goto end_advance;

	newdent = d_lookup(dentry, qname);

	if (!newdent) {
		newdent = d_alloc(dentry, qname);
		if (!newdent)
			goto end_advance;
	} else {
		hashed = 1;
		memcpy((char *) newdent->d_name.name, qname->name,
		       newdent->d_name.len);
	}

	if (!newdent->d_inode) {
		shfs_renew_times(newdent);
		entry->f_ino = iunique(inode->i_sb, 2);
		newino = shfs_iget(inode->i_sb, entry);
		if (newino) {
			shfs_new_dentry(newdent);
			d_instantiate(newdent, newino);
			if (!hashed)
				d_rehash(newdent);
		}
	} else {
		shfs_set_inode_attr(newdent->d_inode, entry);
	}

        if (newdent->d_inode) {
		ino = newdent->d_inode->i_ino;
		newdent->d_fsdata = (void *) ctl.fpos;
		shfs_new_dentry(newdent);
	}

	if (ctl.idx >= SHFS_DIRCACHE_SIZE) {
		if (ctl.page) {
			kunmap(ctl.page);
			SetPageUptodate(ctl.page);
			unlock_page(ctl.page);
			page_cache_release(ctl.page);
		}
		ctl.cache = NULL;
		ctl.idx  -= SHFS_DIRCACHE_SIZE;
		ctl.ofs  += 1;
		ctl.page  = grab_cache_page(&inode->i_data, ctl.ofs);
		if (ctl.page)
			ctl.cache = kmap(ctl.page);
	}
	if (ctl.cache) {
		ctl.cache->dentry[ctl.idx] = newdent;
		valid = 1;
	}
	dput(newdent);

end_advance:
	if (!valid)
		ctl.valid = 0;
	if (!ctl.filled && (ctl.fpos == filp->f_pos)) {
		if (!ino)
			ino = find_inode_number(dentry, qname);
		if (!ino)
			ino = iunique(inode->i_sb, 2);
		ctl.filled = filldir(dirent, qname->name, qname->len,
				     filp->f_pos, ino, DT_UNKNOWN);
		if (!ctl.filled)
			filp->f_pos += 1;
	}
	ctl.fpos += 1;
	ctl.idx  += 1;
	*ctrl = ctl;
	return (ctl.valid || !ctl.filled);
}
