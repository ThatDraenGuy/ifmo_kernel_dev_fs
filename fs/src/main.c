
#include "common.h"
#include "linux/fs.h"
#include "linux/module.h"
#include "fs.h"
#include "linux/moduleparam.h"

char *disk_name;
module_param(disk_name, charp, 0000);

unsigned int snd_super_block_offset = 64;
module_param(snd_super_block_offset, uint, 0000);

unsigned int max_filename_len = 16;
module_param(max_filename_len, uint, 0000);

unsigned int max_file_sectors_count = 10;
module_param(max_file_sectors_count, uint, 0000);

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
