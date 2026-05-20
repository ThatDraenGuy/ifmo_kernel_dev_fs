#ifndef _HAHAFS_HASH_H
#define _HAHAFS_HASH_H

#include "linux/fs.h"
#include "linux/types.h"
#include "superblock.h"

uint32_t haha_hash(char *data, ssize_t size);

uint32_t sb_hash(struct hahafs_super_block *sb);
int update_file_hash(struct file *file);

#endif //_HAHAFS_HASH_H