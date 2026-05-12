#ifndef _HAHAFS_INODE_H
#define _HAHAFS_INODE_H

#include "linux/fs.h"
#include "linux/types.h"

#define HAHAFS_ROOT_INODE 0

struct inode *hahafs_alloc_inode(struct super_block *sb);
void hahafs_destroy_inode(struct inode *inode);
struct inode *hahafs_iget(struct super_block *sb, unsigned long ino);

#define NO_SKIP 0
struct hahafs_extent {
	sector_t start;
	sector_t skip;
};

struct hahafs_inode_info {
	struct inode inode;
	struct hahafs_extent extent;
};

#endif //_HAHAFS_INODE_H