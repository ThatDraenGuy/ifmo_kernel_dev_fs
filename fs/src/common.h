#ifndef _HAHAFS_COMMON_H
#define _HAHAFS_COMMON_H

#include "linux/kern_levels.h"

#define LOG_ERR KERN_ALERT "HAHAFS: "
#define LOG_INFO KERN_INFO "HAHAFS: "
#define HAHAFS_VERSION 1

extern char *disk_name;
extern unsigned int snd_super_block_offset;
extern unsigned int max_filename_len;
extern unsigned int max_file_sectors_count;

#endif //_HAHAFS_COMMON_H