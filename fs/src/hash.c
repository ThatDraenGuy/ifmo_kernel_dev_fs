
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "inode.h"
#include "linux/buffer_head.h"
#include "linux/container_of.h"
#include "linux/fs.h"
#include "linux/types.h"

static uint64_t hash_impl(uint64_t base, char *data, ssize_t size)
{
	for (ssize_t i = 0; i < size; i++) {
		base ^= (unsigned char)(*(data + i));
		base *= 0x100000001b3ULL;
	}
	return base;
}

uint32_t haha_hash(char *data, ssize_t size)
{
	uint64_t h = 0xcbf29ce484222325ULL;

	h = hash_impl(h, data, size);
	/* Fold high 32 bits into low 32 bits to mix the full 64-bit result */
	return (uint32_t)(h ^ (h >> 32));
}

uint32_t sb_hash(struct hahafs_super_block *sb)
{
	return haha_hash(((char *)sb) +
				 offsetofend(struct hahafs_super_block, hash),
			 sizeof(struct hahafs_super_block) -
				 offsetofend(struct hahafs_super_block, hash));
}

int update_file_hash(struct file *file)
{
	struct buffer_head *buf;
	struct inode *inode = file_inode(file);
	struct hahafs_inode_info *hii =
		container_of(inode, struct hahafs_inode_info, inode);
	struct super_block *sb = inode->i_sb;
	loff_t file_size = inode->i_size;
	sector_t stop = hii->extent.start + file_size / HAHAFS_BLOCK_SIZE;

	if (hii->extent.skip < stop)
		stop++;

	sector_t remaining = file_size % HAHAFS_BLOCK_SIZE;
	uint64_t h = 0xcbf29ce484222325ULL;

	for (sector_t block_idx = hii->extent.start; block_idx < stop;
	     block_idx++) {
		if (block_idx == hii->extent.skip)
			block_idx++;

		buf = sb_bread(sb, block_idx);
		if (!buf) {
			printk(LOG_ERR "error reading file\n");
			return -EIO;
		}
		h = hash_impl(h, buf->b_data, HAHAFS_BLOCK_SIZE);
		brelse(buf);
	}

	if (remaining != 0) {
		buf = sb_bread(sb, stop);
		if (!buf) {
			printk(LOG_ERR "error reading file\n");
			return -EIO;
		}
		h = hash_impl(h, buf->b_data, remaining);
		brelse(buf);
	}

	hii->hash = (uint32_t)(h ^ (h >> 32));
	return 0;
}
