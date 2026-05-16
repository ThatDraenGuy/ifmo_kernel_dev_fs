
#include "fs.h"
#include "common.h"
#include "linux/dcache.h"
#include "linux/fs.h"
#include "superblock.h"

static struct dentry *hahafs_mount(struct file_system_type *fs_type, int flags,
				   const char *dev_name, void *data)
{
	struct dentry *res =
		mount_bdev(fs_type, flags, dev_name, data, hahafs_fill_super);
	printk(LOG_INFO "successfully mounted fs\n");
	return res;
}

struct file_system_type hahafs_fs_type = { .owner = THIS_MODULE,
					   .name = "hahafs",
					   .mount = hahafs_mount,
					   .kill_sb = hahafs_kill_super,
					   .fs_flags = FS_REQUIRES_DEV };
