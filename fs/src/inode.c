
#include "inode.h"
#include "asm-generic/errno-base.h"
#include "common.h"
#include "file.h"
#include "linux/buffer_head.h"
#include "linux/container_of.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/fs_types.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/stat.h"
#include "linux/types.h"
#include "superblock.h"

static int hahafs_iterate(struct file *dir, struct dir_context *ctx)
{
	struct inode *inode = file_inode(dir);
	struct super_block *sb = inode->i_sb;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;

	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	while (ctx->pos < sb_info->files_count) {
		sector_t block_idx = inode_block_idx(sb_info, ctx->pos);
		struct buffer_head *buf = sb_bread(sb, block_idx);

		if (!buf) {
			printk(LOG_ERR "error reading from disk\n");
			return -EIO;
		}

		struct hahafs_inode *disk_inode =
			(struct hahafs_inode
				 *)(buf->b_data +
				    (sizeof(struct hahafs_inode) +
				     sb_info->file_name_len) *
					    (ctx->pos %
					     sb_info->inodes_per_block));
		if (!dir_emit(ctx, disk_inode->name, sb_info->file_name_len,
			      ctx->pos + 1, DT_REG)) {
			brelse(buf);
			return 0;
		}
		brelse(buf);
		ctx->pos++;
	}
	return 0;
}

static const struct file_operations hahafs_dir_ops = {
	.iterate_shared = hahafs_iterate,
};

static struct hahafs_extent get_file_extent(struct super_block *sb,
					    unsigned long file_idx)
{
	struct hahafs_sb_info *sb_info = sb->s_fs_info;
	struct hahafs_extent extent;

	extent.start = (INO_FIRST_SECTOR +
			sb_info->files_count / sb_info->inodes_per_block + 1) +
		       file_idx * sb_info->file_sector_count;
	extent.skip = extent.start + sb_info->file_sector_count;

	if (extent.start >= sb_info->snd_superblock_offset)
		extent.start += 1;
	else if (extent.skip > sb_info->snd_superblock_offset)
		extent.skip = sb_info->snd_superblock_offset;

	return extent;
}

struct inode *hahafs_alloc_inode(struct super_block *sb)
{
	struct hahafs_inode_info *hii;

	hii = kzalloc(sizeof(struct hahafs_inode_info), GFP_KERNEL);
	if (!hii)
		return NULL;

	inode_init_once(&hii->inode);
	return &hii->inode;
}

void hahafs_destroy_inode(struct inode *inode)
{
	kfree(container_of(inode, struct hahafs_inode_info, inode));
}

int hahafs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct buffer_head *buf;
	struct super_block *sb = inode->i_sb;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;
	struct hahafs_inode_info *hii =
		container_of(inode, struct hahafs_inode_info, inode);
	unsigned long file_idx = inode->i_ino - 1;
	struct hahafs_inode *disk_inode;

	sector_t block_idx = inode_block_idx(sb_info, file_idx);
	buf = sb_bread(sb, block_idx);
	if (!buf) {
		printk(LOG_ERR "error reading file\n");
		return -ENOMEM;
	}
	disk_inode =
		(struct hahafs_inode *)(buf->b_data +
					(sizeof(struct hahafs_inode) +
					 sb_info->file_name_len) *
						(file_idx %
						 sb_info->inodes_per_block));

	disk_inode->size = inode->i_size;
	disk_inode->hash = hii->hash; //TODO recalc hash

	mark_buffer_dirty(buf);
	brelse(buf);
	return 0;
}

struct inode *hahafs_iget(struct super_block *sb, unsigned long ino)
{
	int ret;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;
	struct inode *inode;
	struct hahafs_inode_info *hii;
	struct buffer_head *buf;

	inode = iget_locked(sb, ino);
	if (!inode) {
		printk(LOG_ERR "error aquiring inode");
		ret = -ENOMEM;
		goto fail;
	}

	if (!(inode->i_state & I_NEW))
		return inode;

	hii = container_of(inode, struct hahafs_inode_info, inode);
	inode->i_mode = (00700 | 00070 | 00007);
	i_uid_write(inode, 0);
	i_gid_write(inode, 0);
	inode_set_ctime_current(inode);

	if (ino == HAHAFS_ROOT_INODE) {
		inode->i_mode |= S_IFDIR;
		inode->i_fop = &hahafs_dir_ops;
	} else {
		unsigned long file_idx = ino - 1;
		struct hahafs_inode *disk_inode;
		sector_t block_idx;

		hii->extent = get_file_extent(sb, file_idx);
		block_idx = inode_block_idx(sb_info, file_idx);
		buf = sb_bread(sb, block_idx);
		if (!buf) {
			printk(LOG_ERR "error reading file metadata");
			ret = -EIO;
			goto cleanup_inode;
		}

		disk_inode = (struct hahafs_inode
				      *)(buf->b_data +
					 (sizeof(struct hahafs_inode) +
					  sb_info->file_name_len) *
						 (file_idx %
						  sb_info->inodes_per_block));

		inode->i_mode |= S_IFREG;
		inode->i_fop = &hahafs_file_ops;
		inode->i_mapping->a_ops = &hahafs_aops;
		inode->i_size = disk_inode->size;
		hii->hash = disk_inode->hash;
	}

	return inode;

cleanup_inode:
	iget_failed(inode);
fail:
	return ERR_PTR(ret);
}
