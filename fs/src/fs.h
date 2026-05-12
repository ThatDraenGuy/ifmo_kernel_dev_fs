#ifndef _HAHAFS_FS_H
#define _HAHAFS_FS_H

#include "linux/fs.h"

extern struct file_system_type hahafs_fs_type;

#define HAHAFS_BLOCK_SIZE 512
#define HAHAFS_FIRST_SB 0

#endif //_HAHAFS_FS_H