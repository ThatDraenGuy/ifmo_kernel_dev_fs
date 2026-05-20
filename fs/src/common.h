#ifndef _HAHAFS_COMMON_H
#define _HAHAFS_COMMON_H

#include "linux/kern_levels.h"

#define LOG_ERR KERN_ALERT "HAHAFS: "
#define LOG_INFO KERN_INFO "HAHAFS: "
#define HAHAFS_VERSION 1

extern char *disk_name;
extern unsigned int snd_superblock_offset;
extern unsigned int file_name_len;
extern unsigned int file_sector_count;

#endif //_HAHAFS_COMMON_H