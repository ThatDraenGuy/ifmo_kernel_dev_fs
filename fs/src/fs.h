#ifndef _HAHAFS_FS_H
#define _HAHAFS_FS_H

#include "linux/fs.h"

extern struct file_system_type hahafs_fs_type;

#define HAHAFS_BLOCK_SIZE 512
#define HAHAFS_FIRST_SB 0

#define IOC_MAGIC 'h'
#define IOC_CLEAN _IO(IOC_MAGIC, 1)
#define IOC_ERASE _IO(IOC_MAGIC, 2)
#define IOC_META _IOWR(IOC_MAGIC, 3, char *)
#define IOC_SECTORS _IOWR(IOC_MAGIC, 4, char *)

#endif //_HAHAFS_FS_H