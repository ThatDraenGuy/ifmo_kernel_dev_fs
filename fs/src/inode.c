
#include "inode.h"
#include "asm-generic/errno-base.h"
#include "common.h"
#include "file.h"
#include "fs.h"
#include "linux/blkdev.h"
#include "linux/buffer_head.h"
#include "linux/container_of.h"
#include "linux/dcache.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/fs_types.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/stat.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "superblock.h"

static struct inode *find_by_name(struct super_block *sb, const char *name)
{
	struct hahafs_sb_info *sb_info = sb->s_fs_info;

	for (unsigned long file_idx = 0; file_idx < sb_info->files_count;
	     file_idx++) {
		sector_t block_idx = inode_block_idx(sb_info, file_idx);
		struct buffer_head *buf = sb_bread(sb, block_idx);

		if (!buf) {
			printk(LOG_ERR "error reading from disk\n");
			return ERR_PTR(-EIO);
		}

		struct hahafs_inode *disk_inode =
			(struct hahafs_inode
				 *)(buf->b_data +
				    inode_size(sb_info) *
					    (file_idx %
					     sb_info->inodes_per_block));

		if (strcmp(name, disk_inode->name) == 0) {
			struct inode *inode = hahafs_iget(sb, file_idx + 1);

			if (IS_ERR(inode))
				return ERR_CAST(inode);
			brelse(buf);
			return inode;
		}

		brelse(buf);
	}

	printk(LOG_ERR "no file named %s\n", name);
	return ERR_PTR(-ENOENT);
}

static long hahafs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	char argbuf[128];
	struct super_block *sb = file->f_inode->i_sb;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;

	switch (cmd) {
	case IOC_CLEAN: {
		for (__u32 file_idx = 0; file_idx < sb_info->files_count;
		     file_idx++) {
			struct inode *inode = hahafs_iget(sb, file_idx + 1);
			struct hahafs_inode_info *hii = container_of(
				inode, struct hahafs_inode_info, inode);

			if (IS_ERR(inode))
				return PTR_ERR(inode);

			inode->i_size = 0;
			hii->hash = 0;

			mark_inode_dirty(inode);
			iput(inode);
		}
		break;
	}
	case IOC_ERASE: {
		sector_t count = bdev_nr_sectors(sb->s_bdev);

		for (sector_t block_idx = 0; block_idx < count; block_idx++) {
			struct buffer_head *buf = sb_bread(sb, block_idx);

			if (!buf) {
				printk(LOG_ERR "error reading from disk\n");
				return -EIO;
			}

			memset(buf->b_data, 0, HAHAFS_BLOCK_SIZE);
			mark_buffer_dirty(buf);
			brelse(buf);
		}
		break;
	}
	case IOC_META: {
		ret = strncpy_from_user(argbuf, (char *)arg,
					sb_info->file_name_len);
		if (ret < 0)
			return -EFAULT;

		struct inode *inode = find_by_name(sb, argbuf);
		if (IS_ERR(inode))
			return PTR_ERR(inode);

		struct hahafs_inode_info *hii =
			container_of(inode, struct hahafs_inode_info, inode);

		sprintf(argbuf, "hash: %u", hii->hash);
		iput(inode);
		ret = copy_to_user((char *)arg, argbuf, strlen(argbuf));
		if (ret)
			return -EFAULT;
		break;
	}
	case IOC_SECTORS: {
		ret = strncpy_from_user(argbuf, (char *)arg,
					sb_info->file_name_len);
		if (ret < 0)
			return -EFAULT;

		struct inode *inode = find_by_name(sb, argbuf);
		if (IS_ERR(inode))
			return PTR_ERR(inode);

		struct hahafs_inode_info *hii =
			container_of(inode, struct hahafs_inode_info, inode);
		sector_t end = hii->extent.start + sb_info->file_sector_count;

		if (hii->extent.start < sb_info->snd_superblock_offset &&
		    end > sb_info->snd_superblock_offset) {
			sprintf(argbuf,
				"Sectors from %llu to %llu with sector %u skipped\n",
				hii->extent.start, end + 1,
				sb_info->snd_superblock_offset);
		} else {
			sprintf(argbuf, "Sectors from %llu to %llu\n",
				hii->extent.start, end);
		}

		iput(inode);
		ret = copy_to_user((char *)arg, argbuf, strlen(argbuf));
		if (ret)
			return -EFAULT;
		break;
	}
	default: {
		return -ENOTTY;
	}
	}
	return 0;
}

static int hahafs_iterate(struct file *dir, struct dir_context *ctx)
{
	struct inode *inode = file_inode(dir);
	struct super_block *sb = inode->i_sb;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;

	if (!S_ISDIR(inode->i_mode)) {
		printk(LOG_ERR "iterate on a file\n");
		return -ENOTDIR;
	}

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
				    inode_size(sb_info) *
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

static struct dentry *hahafs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags)
{
	struct super_block *sb = dentry->d_sb;

	dentry->d_op = sb->s_root->d_op;

	struct inode *inode = find_by_name(sb, dentry->d_name.name);

	if (IS_ERR(inode))
		return ERR_CAST(inode);
	d_add(dentry, inode);
	dget(dentry);
	return dentry;
}

static const struct file_operations hahafs_dir_ops = {
	.read = generic_read_dir,
	.iterate_shared = hahafs_iterate,
	.unlocked_ioctl = hahafs_ioctl,
};

static const struct inode_operations hahafs_dir_inode_opds = {
	.lookup = hahafs_lookup,
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
	if (inode->i_ino == HAHAFS_ROOT_INODE)
		return 0;
	printk(LOG_INFO "writing inode %lu\n", inode->i_ino);

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
					inode_size(sb_info) *
						(file_idx %
						 sb_info->inodes_per_block));

	disk_inode->size = inode->i_size;
	disk_inode->hash = hii->hash;

	mark_buffer_dirty(buf);
	brelse(buf);
	printk(LOG_INFO "wrote inode %lu\n with size %lld; hash %u",
	       inode->i_ino, inode->i_size, hii->hash);
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
		inode->i_op = &hahafs_dir_inode_opds;
		printk(LOG_INFO "got root inode");
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
					 inode_size(sb_info) *
						 (file_idx %
						  sb_info->inodes_per_block));

		inode->i_mode |= S_IFREG;
		inode->i_fop = &hahafs_file_ops;
		inode->i_size = disk_inode->size;
		hii->hash = disk_inode->hash;
		brelse(buf);
		printk(LOG_INFO "got file (#%lu) inode", ino);
	}

	unlock_new_inode(inode);
	return inode;

cleanup_inode:
	iget_failed(inode);
fail:
	return ERR_PTR(ret);
}
