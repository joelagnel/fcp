/*
 * linux/fs/ext2/ioctl.c
 *
 * Copyright (C) 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 */

#include "ext2.h"
#include <linux/capability.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/smp_lock.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include "xattr.h"

// int ext2_update_inode(struct inode * inode, int do_sync);

int ext2_ioctl (struct inode * inode, struct file * filp, unsigned int cmd,
		unsigned long arg)
{
	struct ext2_inode_info *ei = EXT2_I(inode);
	unsigned int flags;
	unsigned short rsv_window_size;
	void *value = NULL;
	struct inode * parent_inode;
	int retval, count = 0, i;

	ext2_debug ("cmd = %u, arg = %lu\n", cmd, arg);

	switch (cmd) {
	case 100:
  	  parent_inode = iget(inode->i_sb, arg);

	  /* set parent of child inode */
	  ext2_debug ("%lu: setting my parent inode to %lu\n", inode->i_ino, arg);
	  ext2_xattr_set(inode, 10, "parent", &arg, sizeof(int), 0);
	  /* also set size and mode of inode to parents' */
          ext2_debug ("%lu: setting my size and mode to parents\n", inode->i_ino);
	  inode->i_size = parent_inode->i_size;
	  inode->i_mode = parent_inode->i_mode;
  	  ext2_sync_inode(inode);

	  /* add to list of parent inode's children */
	  retval = ext2_xattr_get(parent_inode, 11, "children", NULL, 0);
	  if (retval > 0)
	    {
	      ext2_debug ("%lu: adding to child list : %lu\n", parent_inode->i_ino, inode->i_ino);
	      count = retval + sizeof(int);
	      value = (int *)kmalloc(count, GFP_KERNEL);
	      if (!value)
		return -ENOMEM;
	      ext2_xattr_get(parent_inode, 11, "children", value, retval);
	      for(i=0; i<retval; i++)
		if ( ((int *)value)[i] == inode->i_ino )
		  {
		    ext2_debug ("%lu: child %lu exists\n", parent_inode->i_ino, inode->i_ino);
		    return 0;
		  }
	      ext2_debug ("%lu: (retval/sizeof(int)) : %d\n", parent_inode->i_ino, (retval/sizeof(int)));
	      ((int *)value)[(retval/sizeof(int))-1] = inode->i_ino;
	      ((int *)value)[(retval/sizeof(int))] = 0;
 	      ext2_xattr_set(parent_inode, 11, "children", value, count, 0);
	      kfree(value);
	    }
	  else
	    {
	      ext2_debug ("%lu: child list doesn't exist, creating.\n", parent_inode->i_ino);
	      count = 2 * sizeof(int);
	      value = (int *)kmalloc(count, GFP_KERNEL);
	      ((int *)value)[0] = inode->i_ino;
	      ((int *)value)[1] = 0; /* end marker*/
              ext2_xattr_set(parent_inode, 11, "children", value, 2 * sizeof(int), 0);
	      kfree(value);
	    }
	  if(parent_inode)
	    {
	      iput(parent_inode);
	    }
	  return 0;

	case 101:
	  retval = ext2_xattr_get(inode, 10, "parent", NULL, 0);
	  if (retval > 0) {
	    value = kmalloc(retval, GFP_KERNEL);
	    if (!value)
	      return -ENOMEM;
	    retval = ext2_xattr_get(inode, 10, "parent", value, retval);
	    if(!value) {
	      return 1;
	    }
  	    ext2_debug ("%lu: parent inode number  = %d\n", inode->i_ino, *((int *)value));
   	    put_user(*((int *)value), (int *)arg);
	    kfree(value);
	  }
	  return 0;

	case 102:
	  retval = ext2_xattr_get(inode, 11, "children", NULL, 0);
	  if (retval > 0) {
	    value = kmalloc(retval, GFP_KERNEL);
	    if (!value)
	      return -ENOMEM;
	    retval = ext2_xattr_get(inode, 11, "children", value, retval);
	    if(!value) {
	      return 1;
	    }
	    if(copy_to_user((void __user *)arg, value, retval))
	      {
		ext2_debug("%lu: get_children: Couldn't copy some bytes to user space.\n", inode->i_ino);
	      }
	    else
	      {
		ext2_debug("%lu: get_children: copied to user space.\n", inode->i_ino);
	      }
	    kfree(value);
	  }
	  else
	    {
	      ext2_debug("%lu: get_children: none found.\n", inode->i_ino);
	    }
	  return 0;

	case EXT2_IOC_GETFLAGS:
		ext2_get_inode_flags(ei);
		flags = ei->i_flags & EXT2_FL_USER_VISIBLE;
		return put_user(flags, (int __user *) arg);
	case EXT2_IOC_SETFLAGS: {
		unsigned int oldflags;

		if (IS_RDONLY(inode))
			return -EROFS;

		if (!is_owner_or_cap(inode))
			return -EACCES;

		if (get_user(flags, (int __user *) arg))
			return -EFAULT;

		if (!S_ISDIR(inode->i_mode))
			flags &= ~EXT2_DIRSYNC_FL;

		mutex_lock(&inode->i_mutex);
		/* Is it quota file? Do not allow user to mess with it */
		if (IS_NOQUOTA(inode)) {
			mutex_unlock(&inode->i_mutex);
			return -EPERM;
		}
		oldflags = ei->i_flags;

		/*
		 * The IMMUTABLE and APPEND_ONLY flags can only be changed by
		 * the relevant capability.
		 *
		 * This test looks nicer. Thanks to Pauline Middelink
		 */
		if ((flags ^ oldflags) & (EXT2_APPEND_FL | EXT2_IMMUTABLE_FL)) {
			if (!capable(CAP_LINUX_IMMUTABLE)) {
				mutex_unlock(&inode->i_mutex);
				return -EPERM;
			}
		}

		flags = flags & EXT2_FL_USER_MODIFIABLE;
		flags |= oldflags & ~EXT2_FL_USER_MODIFIABLE;
		ei->i_flags = flags;
		mutex_unlock(&inode->i_mutex);

		ext2_set_inode_flags(inode);
		inode->i_ctime = CURRENT_TIME_SEC;
		mark_inode_dirty(inode);
		return 0;
	}
	case EXT2_IOC_GETVERSION:
		return put_user(inode->i_generation, (int __user *) arg);
	case EXT2_IOC_SETVERSION:
		if (!is_owner_or_cap(inode))
			return -EPERM;
		if (IS_RDONLY(inode))
			return -EROFS;
		if (get_user(inode->i_generation, (int __user *) arg))
			return -EFAULT;	
		inode->i_ctime = CURRENT_TIME_SEC;
		mark_inode_dirty(inode);
		return 0;
	case EXT2_IOC_GETRSVSZ:
		if (test_opt(inode->i_sb, RESERVATION)
			&& S_ISREG(inode->i_mode)
			&& ei->i_block_alloc_info) {
			rsv_window_size = ei->i_block_alloc_info->rsv_window_node.rsv_goal_size;
			return put_user(rsv_window_size, (int __user *)arg);
		}
		return -ENOTTY;
	case EXT2_IOC_SETRSVSZ: {

		if (!test_opt(inode->i_sb, RESERVATION) ||!S_ISREG(inode->i_mode))
			return -ENOTTY;

		if (IS_RDONLY(inode))
			return -EROFS;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EACCES;

		if (get_user(rsv_window_size, (int __user *)arg))
			return -EFAULT;

		if (rsv_window_size > EXT2_MAX_RESERVE_BLOCKS)
			rsv_window_size = EXT2_MAX_RESERVE_BLOCKS;

		/*
		 * need to allocate reservation structure for this inode
		 * before set the window size
		 */
		/*
		 * XXX What lock should protect the rsv_goal_size?
		 * Accessed in ext2_get_block only.  ext3 uses i_truncate.
		 */
		mutex_lock(&ei->truncate_mutex);
		if (!ei->i_block_alloc_info)
			ext2_init_block_alloc_info(inode);

		if (ei->i_block_alloc_info){
			struct ext2_reserve_window_node *rsv = &ei->i_block_alloc_info->rsv_window_node;
			rsv->rsv_goal_size = rsv_window_size;
		}
		mutex_unlock(&ei->truncate_mutex);
		return 0;
	}
	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long ext2_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	int ret;

	/* These are just misnamed, they actually get/put from/to user an int */
	switch (cmd) {
	case EXT2_IOC32_GETFLAGS:
		cmd = EXT2_IOC_GETFLAGS;
		break;
	case EXT2_IOC32_SETFLAGS:
		cmd = EXT2_IOC_SETFLAGS;
		break;
	case EXT2_IOC32_GETVERSION:
		cmd = EXT2_IOC_GETVERSION;
		break;
	case EXT2_IOC32_SETVERSION:
		cmd = EXT2_IOC_SETVERSION;
		break;
	default:
		return -ENOIOCTLCMD;
	}
	lock_kernel();
	ret = ext2_ioctl(inode, file, cmd, (unsigned long) compat_ptr(arg));
	unlock_kernel();
	return ret;
}
#endif
