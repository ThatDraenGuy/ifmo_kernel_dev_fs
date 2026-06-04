
#include "superblock.h"
#include "asm-generic/errno-base.h"
#include "common.h"
#include "fs.h"
#include "hash.h"
#include "inode.h"
#include "linux/blkdev.h"
#include "linux/buffer_head.h"
#include "linux/dcache.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/stddef.h"
#include "linux/types.h"

static void hahafs_put_super(struct super_block *sb)
{
}

static const struct super_operations hahafs_super_ops = {
	.alloc_inode = hahafs_alloc_inode,
	.destroy_inode = hahafs_destroy_inode,
	.put_super = hahafs_put_super,
	.write_inode = hahafs_write_inode
};

static bool check_sb_equal(struct hahafs_super_block *fst_haha_sb,
			   struct hahafs_super_block *snd_haha_sb)
{
	if (fst_haha_sb->hash != snd_haha_sb->hash) {
		printk(LOG_ERR "superblock hash mismatch\n");
		return false;
	}
	if (fst_haha_sb->version != snd_haha_sb->version) {
		printk(LOG_ERR "superblock version mismatch\n");
		return false;
	}
	if (fst_haha_sb->file_sector_count != snd_haha_sb->file_sector_count) {
		printk(LOG_ERR "superblock file_sector_count mismatch\n");
		return false;
	}
	if (fst_haha_sb->file_name_len != snd_haha_sb->file_name_len) {
		printk(LOG_ERR "superblock file_name_len mismatch\n");
		return false;
	}
	if (fst_haha_sb->snd_superblock_offset !=
	    snd_haha_sb->snd_superblock_offset) {
		printk(LOG_ERR "superblock snd_superblock_offset mismatch\n");
		return false;
	}
	return true;
}
static bool check_module_params(struct hahafs_super_block *haha_sb)
{
	if (haha_sb->file_sector_count != file_sector_count) {
		printk(LOG_ERR
		       "superblock file_sector_count mismatch with module param\n");
		return false;
	}
	if (haha_sb->file_name_len != file_name_len) {
		printk(LOG_ERR
		       "superblock file_name_len mismatch with module param\n");
		return false;
	}
	if (haha_sb->snd_superblock_offset != snd_superblock_offset) {
		printk(LOG_ERR
		       "superblock snd_superblock_offset mismatch with module param\n");
		return false;
	}
	return true;
}

int hahafs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct hahafs_sb_info *sb_info;
	struct hahafs_super_block *fst_haha_sb;
	struct hahafs_super_block *snd_haha_sb;
	struct buffer_head *sb1_buf;
	struct buffer_head *sb2_buf;
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
	sb1_buf = sb_bread(sb, HAHAFS_FIRST_SB);
	if (!sb1_buf) {
		printk(LOG_ERR "error reading superblock1\n");
		goto cleanup_sb_info;
	}
	fst_haha_sb = (struct hahafs_super_block *)sb1_buf->b_data;
	if (fst_haha_sb->magic != HAHAFS_SB_MAGIC) {
		printk(LOG_ERR "invalid magic superblock1\n");
		goto cleanup_sb1;
	}
	if (fst_haha_sb->hash != sb_hash(fst_haha_sb)) {
		printk(LOG_ERR "hash mismatch superblock1\n");
		goto cleanup_sb1;
	}

	//get second on-disk superblock
	sb2_buf = sb_bread(sb, snd_superblock_offset);
	if (!sb2_buf) {
		printk(LOG_ERR "error reading superblock2\n");
		goto cleanup_sb_info;
	}
	snd_haha_sb = (struct hahafs_super_block *)sb2_buf->b_data;
	if (snd_haha_sb->magic != HAHAFS_SB_MAGIC) {
		printk(LOG_ERR "invalid magic superblock2\n");
		goto cleanup_sb1;
	}
	if (snd_haha_sb->hash != sb_hash(snd_haha_sb)) {
		printk(LOG_ERR "hash mismatch superblock2\n");
		goto cleanup_sb1;
	}

	if (!check_sb_equal(fst_haha_sb, snd_haha_sb))
		goto cleanup_sb2;
	if (!check_module_params(fst_haha_sb))
		goto cleanup_sb2;

	//store superblock data
	sb_info->version = fst_haha_sb->version;
	sb_info->file_sector_count = fst_haha_sb->file_sector_count;
	sb_info->file_name_len = fst_haha_sb->file_name_len;
	sb_info->snd_superblock_offset = fst_haha_sb->snd_superblock_offset;

	sb_info->inodes_per_block =
		HAHAFS_BLOCK_SIZE /
		(sizeof(struct hahafs_inode) + sb_info->file_name_len);

	sector_t free_sectors =
		bdev_nr_sectors(sb->s_bdev) - 2; //2 сектора на суперблоки
	// наборы из полностью заполненного сектора под иноды файлов и секторов под данные этих файлов
	__u32 fully_filled =
		free_sectors /
		(1 + sb_info->file_sector_count * sb_info->inodes_per_block);
	// оставшиеся сектора
	__u32 remaining =
		free_sectors %
		(1 + sb_info->file_sector_count * sb_info->inodes_per_block);

	// полные наборы + последний неполностью заполненный сектор под иноды файлов
	sb_info->files_count = fully_filled * sb_info->inodes_per_block +
			       (remaining - 1) / sb_info->file_sector_count;
	sb_info->is_invalid = false;

	sb->s_magic = HAHAFS_SB_MAGIC;
	sb->s_op = &hahafs_super_ops;
	sb->s_maxbytes = HAHAFS_BLOCK_SIZE * file_sector_count;

	root_inode = hahafs_iget(sb, HAHAFS_ROOT_INODE);
	if (IS_ERR(root_inode))
		goto cleanup_sb2;

	root_dentry = d_make_root(root_inode);
	if (!root_dentry)
		goto cleanup_inode;
	sb->s_root = root_dentry;

	brelse(sb2_buf);
	brelse(sb1_buf);
	printk(LOG_INFO "successfully filled superblock\n");
	return 0;

cleanup_inode:
	iput(root_inode);
cleanup_sb2:
	brelse(sb2_buf);
cleanup_sb1:
	brelse(sb1_buf);
cleanup_sb_info:
	sb->s_fs_info = NULL;
	kfree(sb_info);
	return ret;
}

void hahafs_kill_super(struct super_block *sb)
{
	kill_block_super(sb);
}
