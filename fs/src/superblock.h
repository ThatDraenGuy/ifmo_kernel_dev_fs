#ifndef _HAHAFS_SUPERBLOCK_H
#define _HAHAFS_SUPERBLOCK_H

#include "asm-generic/int-ll64.h"
#include "linux/fs.h"

int hahafs_fill_super(struct super_block *sb, void *data, int silent);
void hahafs_kill_super(struct super_block *sb);

struct hahafs_sb_info {
	__u8 version;
	__u32 file_sector_count;
	__u32 snd_superblock_offset;
	struct buffer_head *sb1_buf;
	struct buffer_head *sb2_buf;
};

//#define HAHAFS_SB_MAGIC 0xB0BAFE77
#define HAHAFS_SB_MAGIC 0xB00B6767

struct hahafs_super_block {
	unsigned long magic;
	__u8 version;
	__u32 file_sector_count;
	__u32 snd_superblock_offset;
};

#endif //_HAHAFS_SUPERBLOCK_H