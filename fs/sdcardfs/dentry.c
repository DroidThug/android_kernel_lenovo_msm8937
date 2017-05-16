/*
 * fs/sdcardfs/dentry.c
 *
 * Copyright (c) 2015 Lenovo Co. Ltd
 *   Authors: liaohs , jixj
                
* This program has been developed as a stackable file system based on
 * the WrapFS which written by 
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "sdcardfs.h"
#include "linux/ctype.h"

/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int sdcardfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	int err = 1;
	struct path parent_lower_path, lower_path;
	struct dentry *parent_dentry = NULL;
	struct dentry *parent_lower_dentry = NULL;
	struct dentry *lower_cur_parent_dentry = NULL;
	struct dentry *lower_dentry = NULL;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	spin_lock(&dentry->d_lock);
	if (IS_ROOT(dentry)) {
		spin_unlock(&dentry->d_lock);
		return 1;
	}
	spin_unlock(&dentry->d_lock);

	/* check uninitialized obb_dentry and  
	 * whether the base obbpath has been changed or not */
	if (is_obbpath_invalid(dentry)) {
		d_drop(dentry);
		return 0;
	}

	parent_dentry = dget_parent(dentry);
	sdcardfs_get_lower_path(parent_dentry, &parent_lower_path);
	sdcardfs_get_real_lower(dentry, &lower_path);
	parent_lower_dentry = parent_lower_path.dentry;
	lower_dentry = lower_path.dentry;
	lower_cur_parent_dentry = dget_parent(lower_dentry);

	spin_lock(&lower_dentry->d_lock);
	if (d_unhashed(lower_dentry)) {
		spin_unlock(&lower_dentry->d_lock);
		d_drop(dentry);
		err = 0;
		goto out;
	}
	spin_unlock(&lower_dentry->d_lock);

	if (parent_lower_dentry != lower_cur_parent_dentry) {
		d_drop(dentry);
		err = 0;
		goto out;
	}

/* ######## ATTENTION ########
~~~ spin_lock(&lower_dentry->d_lock);  //here call spin_lock , will cause follow DEADLOCK ~~

4>[   38.406030]-(1)[829:zygote] (&(&dentry->d_lock)->rlock){+.+...}, at: [<c026a6c4>] sdcardfs_d_revalidate+0x1b8/0x258
<4>[   38.406057]-(1)[829:zygote]
<4>[   38.406062]-(1)[829:zygote]but task is already holding lock:
<4>[   38.406069]-(1)[829:zygote] (&(&dentry->d_lock)->rlock){+.+...}, at: [<c026a6bc>] sdcardfs_d_revalidate+0x1b0/0x258
<4>[   38.406094]-(1)[829:zygote]
<4>[   38.406098]-(1)[829:zygote]other info that might help us debug this:
<4>[   38.406107]-(1)[829:zygote] Possible unsafe locking scenario:
<4>[   38.406113]-(1)[829:zygote]
<4>[   38.406119]-(1)[829:zygote]       CPU0
<4>[   38.406125]-(1)[829:zygote]       ----
<4>[   38.406132]-(1)[829:zygote]  lock(&(&dentry->d_lock)->rlock);
<4>[   38.406143]-(1)[829:zygote]  lock(&(&dentry->d_lock)->rlock);
<4>[   38.406153]-(1)[829:zygote]
<4>[   38.406158]-(1)[829:zygote] *** DEADLOCK ***
<4>[   38.406163]-(1)[829:zygote]
<4>[   38.406169]-(1)[829:zygote] May be due to missing lock nesting notation
 */
	if (dentry < lower_dentry) {
		spin_lock(&dentry->d_lock);
		spin_lock_nested(&lower_dentry->d_lock, 1);
	} else {
		spin_lock(&lower_dentry->d_lock);
		spin_lock_nested(&dentry->d_lock, 1);
	}

	if (dentry->d_name.len != lower_dentry->d_name.len) {
		__d_drop(dentry);
		err = 0;
	} else if (strncasecmp(dentry->d_name.name, lower_dentry->d_name.name,
				dentry->d_name.len) != 0) {
		__d_drop(dentry);
		err = 0;
	}

	if (dentry < lower_dentry) {
		spin_unlock(&lower_dentry->d_lock);
		spin_unlock(&dentry->d_lock);
	} else {
		spin_unlock(&dentry->d_lock);
		spin_unlock(&lower_dentry->d_lock);
	}

out:
	dput(parent_dentry);
	dput(lower_cur_parent_dentry);
	sdcardfs_put_lower_path(parent_dentry, &parent_lower_path);
	sdcardfs_put_real_lower(dentry, &lower_path);
	return err;
}

static void sdcardfs_d_release(struct dentry *dentry)
{
	/* release and reset the lower paths */
	if(has_graft_path(dentry)) {
		sdcardfs_put_reset_orig_path(dentry);
	}
	sdcardfs_put_reset_lower_path(dentry);
	free_dentry_private_data(dentry);
	return;
}

 #if 0
static int sdcardfs_hash_ci(const struct dentry *dentry, 
				const struct inode *inode, struct qstr *qstr)
{
	/* 
	 * This function is copy of vfat_hashi.
	 * FIXME Should we support national language?
	 *       Refer to vfat_hashi()
	 * struct nls_table *t = MSDOS_SB(dentry->d_sb)->nls_io;
	 */
	const unsigned char *name;
	unsigned int len;
	unsigned long hash;

	name = qstr->name;
	//len = vfat_striptail_len(qstr);
	len = qstr->len; 

	hash = init_name_hash();
	while (len--)
		//hash = partial_name_hash(nls_tolower(t, *name++), hash);
		hash = partial_name_hash(tolower(*name++), hash);
	qstr->hash = end_name_hash(hash);

	return 0;
}

/*
 * Case insensitive compare of two vfat names.
 */

static int sdcardfs_cmp_ci(const struct dentry *parent, 
		const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	/* This function is copy of vfat_cmpi */
	// FIXME Should we support national language? 
	//struct nls_table *t = MSDOS_SB(parent->d_sb)->nls_io;
	//unsigned int alen, blen;

	/* A filename cannot end in '.' or we treat it like it has none */
	/*
	alen = vfat_striptail_len(name);
	blen = __vfat_striptail_len(len, str);
	if (alen == blen) {
		if (nls_strnicmp(t, name->name, str, alen) == 0)
			return 0;
	}
	*/
	if (name->len == len) {
		if (strncasecmp(name->name, str, len) == 0)
			return 0; 
	}
	return 1;
}
#endif

/* directly from fs/fat/namei_vfat.c */
static unsigned int __vfat_striptail_len(unsigned int len, const char *name)
{
	while (len && name[len - 1] == '.')
		len--;
	return len;
}

static unsigned int vfat_striptail_len(const struct qstr *qstr)
{
	return __vfat_striptail_len(qstr->len, qstr->name);
}



/* based on vfat_hashi() in fs/fat/namei_vfat.c (no code pages) */
static int sdcardfs_hash_ci(const struct dentry *dentry, struct qstr *qstr)
{
	const unsigned char *name;
	unsigned int len;
	unsigned long hash;

	name = qstr->name;
	len = vfat_striptail_len(qstr);

	hash = init_name_hash();
	while (len--)
		hash = partial_name_hash(tolower(*name++), hash);
	qstr->hash = end_name_hash(hash);

	return 0;
}



/* based on vfat_cmpi() in fs/fat/namei_vfat.c (no code pages) */
static int sdcardfs_cmp_ci(const struct dentry *parent,
			   const struct dentry *dentry, unsigned int len,
			   const char *str, const struct qstr *name)
{
	unsigned int alen, blen;

	/* A filename cannot end in '.' or we treat it like it has none */
	alen = vfat_striptail_len(name);
	blen = __vfat_striptail_len(len, str);
	if (alen == blen) {
		if (strncasecmp(name->name, str, alen) == 0)
			return 0;
	}
	return 1;
}



const struct dentry_operations sdcardfs_ci_dops = {
	.d_revalidate	= sdcardfs_d_revalidate,
	.d_release	= sdcardfs_d_release,
	.d_hash 	= sdcardfs_hash_ci, 
	.d_compare	= sdcardfs_cmp_ci,
};

