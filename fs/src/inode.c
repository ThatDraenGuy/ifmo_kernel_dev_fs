
#include "inode.h"
#include "common.h"
#include "linux/container_of.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/stat.h"
#include "linux/types.h"
#include "superblock.h"

#define INO_START_IDX 1
#define INO_FIRST_SECTOR 1

static struct hahafs_extent get_file_extent(struct super_block *sb,
					    unsigned long ino)
{
	struct hahafs_sb_info *sb_info = sb->s_fs_info;
	struct hahafs_extent extent;

	extent.start = INO_FIRST_SECTOR +
		       (ino - INO_START_IDX) * sb_info->file_sector_count;
	extent.skip = extent.start + sb_info->file_sector_count;

	if (extent.start >= sb_info->snd_superblock_offset) {
		extent.start += 1;
	} else if (extent.skip > sb_info->snd_superblock_offset) {
		extent.skip = sb_info->snd_superblock_offset;
	}

	return extent;
}

static const struct file_operations hahafs_fops = {

};

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

struct inode *hahafs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;

	inode = iget_locked(sb, ino);
	if (!inode) {
		printk(LOG_ERR "error aquiring inode");
		return NULL;
	}

	if (!(inode->i_state & I_NEW))
		return inode;

	inode_set_ctime_current(inode);
	return inode;
}
