
#include "common.h"
#include "linux/fs.h"
#include "linux/module.h"
#include "fs.h"
#include "linux/moduleparam.h"

char *disk_name;
module_param(disk_name, charp, 0000);

unsigned int snd_superblock_offset = 64;
module_param(snd_superblock_offset, uint, 0000);

unsigned int file_name_len = 16;
module_param(file_name_len, uint, 0000);

unsigned int file_sector_count = 10;
module_param(file_sector_count, uint, 0000);

static int init(void)
{
	int ret;

	ret = register_filesystem(&hahafs_fs_type);
	if (ret) {
		printk(LOG_ERR "register_filesystem failed\n");
		return ret;
	}
	return 0;
}
static void exit(void)
{
	unregister_filesystem(&hahafs_fs_type);
}

module_init(init);
module_exit(exit);
MODULE_LICENSE("GPL");
