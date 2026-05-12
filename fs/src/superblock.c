
#include "superblock.h"
#include "asm-generic/errno-base.h"
#include "common.h"
#include "fs.h"
#include "inode.h"
#include "linux/blkdev.h"
#include "linux/buffer_head.h"
#include "linux/dcache.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"

static void hahafs_put_super(struct super_block *sb)
{
	struct hahafs_sb_info *sb_info = sb->s_fs_info;

	mark_buffer_dirty(sb_info->sb1_buf);
	brelse(sb_info->sb1_buf);
	mark_buffer_dirty(sb_info->sb2_buf);
	brelse(sb_info->sb2_buf);
}

static const struct super_operations hahafs_super_ops = {
	.alloc_inode = hahafs_alloc_inode,
	.destroy_inode = hahafs_destroy_inode,
	.put_super = hahafs_put_super
};

int hahafs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct hahafs_sb_info *sb_info;
	struct hahafs_super_block *fst_haha_sb;
	struct hahafs_super_block *snd_haha_sb;
	struct inode *root_inode;
	struct dentry *root_dentry;
	int ret = -EINVAL;

	sb_info = kzalloc(sizeof(struct hahafs_sb_info), GFP_KERNEL);
	if (!sb_info)
		return -ENOMEM;
	sb->s_fs_info = sb_info;

	if (!sb_set_blocksize(sb, HAHAFS_BLOCK_SIZE)) {
		printk(LOG_ERR "bad block size\n");
		goto cleanup_sb_info;
	}

	//get first on-disk superblock
	sb_info->sb1_buf = sb_bread(sb, HAHAFS_FIRST_SB);
	if (!sb_info->sb1_buf) {
		printk(LOG_ERR "error reading superblock1\n");
		goto cleanup_sb_info;
	}

	fst_haha_sb = (struct hahafs_super_block *)sb_info->sb1_buf->b_data;
	if (fst_haha_sb->magic != HAHAFS_SB_MAGIC) {
		printk(LOG_ERR "invalid magic superblock1\n");
		goto cleanup_sb1;
	}

	//get second on-disk superblock
	sb_info->sb2_buf = sb_bread(sb, snd_super_block_offset);
	if (!sb_info->sb2_buf) {
		printk(LOG_ERR "error reading superblock2\n");
		goto cleanup_sb_info;
	}

	snd_haha_sb = (struct hahafs_super_block *)sb_info->sb2_buf->b_data;
	if (snd_haha_sb->magic != HAHAFS_SB_MAGIC) {
		printk(LOG_ERR "invalid magic superblock2\n");
		goto cleanup_sb1;
	}

	//check superblocks equal
	if (fst_haha_sb->version != snd_haha_sb->version) {
		printk(LOG_ERR "superblock version mismatch\n");
		goto cleanup_sb2;
	}

	//store superblock data
	sb_info->version = fst_haha_sb->version;
	sb_info->file_sector_count = fst_haha_sb->file_sector_count;

	sb->s_magic = HAHAFS_SB_MAGIC;
	sb->s_op = &hahafs_super_ops;
	sb->s_maxbytes = HAHAFS_BLOCK_SIZE * max_file_sectors_count;

	root_inode = hahafs_iget(sb, HAHAFS_ROOT_INODE);
	if (!root_inode)
		goto cleanup_sb2;

	root_dentry = d_make_root(root_inode);
	if (!root_dentry)
		goto cleanup_sb2;
	sb->s_root = root_dentry;

	return 0;

cleanup_sb2:
	brelse(sb_info->sb2_buf);
cleanup_sb1:
	brelse(sb_info->sb1_buf);
cleanup_sb_info:
	sb->s_fs_info = NULL;
	kfree(sb_info);
	return ret;
}

void hahafs_kill_super(struct super_block *sb)
{
	kill_block_super(sb);
}
