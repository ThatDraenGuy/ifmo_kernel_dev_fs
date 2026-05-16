#ifndef _HAHAFS_INODE_H
#define _HAHAFS_INODE_H

#include "linux/fs.h"
#include "linux/types.h"
#include "linux/writeback.h"

#define HAHAFS_ROOT_INODE 0

struct inode *hahafs_alloc_inode(struct super_block *sb);
void hahafs_destroy_inode(struct inode *inode);
int hahafs_write_inode(struct inode *inode, struct writeback_control *wbc);
struct inode *hahafs_iget(struct super_block *sb, unsigned long ino);

struct hahafs_extent {
	sector_t start;
	sector_t skip;
};

struct hahafs_inode_info {
	struct inode inode;
	struct hahafs_extent extent;
	uint32_t hash;
};

struct hahafs_inode {
	__u32 hash;
	__u32 size;
	char name[];
};

#define INO_FIRST_SECTOR 1
#define inode_block_idx(sb_info, file_idx) \
	INO_FIRST_SECTOR + (file_idx / sb_info->inodes_per_block)

#endif //_HAHAFS_INODE_H