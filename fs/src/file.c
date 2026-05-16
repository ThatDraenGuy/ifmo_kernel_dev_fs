
#include "file.h"
#include "asm-generic/errno-base.h"
#include "common.h"
#include "fs.h"
#include "inode.h"
#include "linux/buffer_head.h"
#include "linux/container_of.h"
#include "linux/fs.h"
#include "linux/init.h"
#include "linux/minmax.h"
#include "linux/mm_types.h"
#include "linux/mpage.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "superblock.h"

static int hahafs_file_get_block(struct inode *inode, sector_t block,
				 struct buffer_head *buf_res, int create)
{
	struct super_block *sb = inode->i_sb;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;
	struct hahafs_inode_info *hii =
		container_of(inode, struct hahafs_inode_info, inode);

	if (block >= sb_info->file_sector_count)
		return -EFBIG;

	sector_t sector = hii->extent.start + block;

	if (sector >= hii->extent.skip)
		sector += 1;

	map_bh(buf_res, sb, sector);
	return 0;
}

static void hahafs_readahead(struct readahead_control *rac)
{
	mpage_readahead(rac, hahafs_file_get_block);
}

static int hahafs_write_begin(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned int len,
			      struct folio **foliop, void **fsdata)
{
	//TODO check blocks num
	return block_write_begin(mapping, pos, len, foliop,
				 hahafs_file_get_block);
}

static int hahafs_write_end(struct file *file, struct address_space *mapping,
			    loff_t pos, unsigned int len, unsigned int copied,
			    struct folio *foliop, void *fsdata)
{
	//TODO hash recalc
	int ret = generic_write_end(file, mapping, pos, len, copied, foliop,
				    fsdata);
	if (ret < len) {
		printk(LOG_ERR "wrote less than requested\n");
		return ret;
	}
	return ret;
}

const struct address_space_operations hahafs_aops = {
	.readahead = hahafs_readahead,
	.write_begin = hahafs_write_begin,
	.write_end = hahafs_write_end
};

static ssize_t hahafs_read(struct file *file, char __user *buf, size_t len,
			   loff_t *ppos)
{
	ssize_t already_read = 0;
	struct inode *inode = file_inode(file);
	struct hahafs_inode_info *hii =
		container_of(inode, struct hahafs_inode_info, inode);
	struct super_block *sb = inode->i_sb;

	if (*ppos > inode->i_size)
		return 0;
	if (*ppos + len > inode->i_size)
		len = inode->i_size - *ppos;

	while (len > 0) {
		sector_t block_idx =
			hii->extent.start + *ppos / HAHAFS_BLOCK_SIZE;
		if (block_idx >= hii->extent.skip)
			block_idx++;

		struct buffer_head *disk_buf = sb_bread(sb, block_idx);

		if (!disk_buf) {
			printk(LOG_ERR "error reading from disk\n");
			return -EIO;
		}

		size_t offset = *ppos % HAHAFS_BLOCK_SIZE;
		size_t to_read = min_t(size_t, len, HAHAFS_BLOCK_SIZE - offset);

		if (copy_to_user(buf + already_read, disk_buf->b_data + offset,
				 to_read)) {
			brelse(disk_buf);
			return -EFAULT;
		}
		brelse(disk_buf);
		already_read += to_read;
		len -= to_read;
		*ppos += to_read;
	}
	return already_read;
}

static ssize_t hahafs_write(struct file *file, const char __user *buf,
			    size_t len, loff_t *ppos)
{
	ssize_t already_written = 0;
	struct inode *inode = file_inode(file);
	struct hahafs_inode_info *hii =
		container_of(inode, struct hahafs_inode_info, inode);
	struct super_block *sb = inode->i_sb;
	struct hahafs_sb_info *sb_info = sb->s_fs_info;

	if (*ppos > inode->i_size)
		return 0;

	len = min_t(size_t, len,
		    HAHAFS_BLOCK_SIZE * sb_info->file_sector_count);

	sector_t block_idx = hii->extent.start + *ppos / HAHAFS_BLOCK_SIZE;

	while (len > 0) {
		if (block_idx >= hii->extent.skip)
			block_idx++;

		struct buffer_head *disk_buf = sb_bread(sb, block_idx);

		if (!disk_buf) {
			printk(LOG_ERR "error reading from disk\n");
			return -EIO;
		}

		size_t offset = *ppos % HAHAFS_BLOCK_SIZE;
		size_t to_write =
			min_t(size_t, len, HAHAFS_BLOCK_SIZE - offset);

		if (copy_from_user(disk_buf->b_data + offset,
				   buf + already_written, to_write)) {
			brelse(disk_buf);
			return -EFAULT;
		}
		mark_buffer_dirty(disk_buf);
		sync_dirty_buffer(disk_buf);
		brelse(disk_buf);

		already_written += to_write;
		len -= to_write;
		*ppos += to_write;
		block_idx = hii->extent.start + *ppos / HAHAFS_BLOCK_SIZE;
	}

	inode->i_size = max(*ppos, inode->i_size);
	mark_inode_dirty(inode);
	return already_written;
}

const struct file_operations hahafs_file_ops = { .owner = THIS_MODULE,
						 .read = hahafs_read,
						 .write = hahafs_write,
						 .llseek = generic_file_llseek,
						 .fsync = generic_file_fsync };
