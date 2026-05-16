
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HAHAFS_BLOCK_SIZE 512
#define HAHAFS_SB_MAGIC 0xB00B6767

struct hahafs_super_block {
	unsigned long magic;
	uint8_t version;
	uint32_t file_sector_count;
	uint32_t file_name_len;
	uint32_t snd_superblock_offset;
};

struct hahafs_inode {
	uint32_t hash;
	uint32_t size;
	char name[];
};

char *inode_buf[128] = {};

int main(int argc, char **argv)
{
	FILE *file;
	char buffer[HAHAFS_BLOCK_SIZE];
	struct hahafs_super_block sb;

	if (argc != 6) {
		// ./haha_init /dev/sda 100 64 16 10
		fprintf(stderr,
			"Usage: %s <disk name> <sectors number> <2nd sb sector> <filename len> <max files sectors>",
			argv[0]);
		return 1;
	}
	char *disk_name = argv[1];
	uint32_t sectors_count = atoi(argv[2]);

	memset(&sb, 0, sizeof(struct hahafs_super_block));
	sb.snd_superblock_offset = atoi(argv[3]);
	sb.file_name_len = atoi(argv[4]);
	sb.file_sector_count = atoi(argv[5]);
	sb.version = 1;
	sb.magic = HAHAFS_SB_MAGIC;

	file = fopen(disk_name, "w+");
	if (!file) {
		goto file_error;
	}

	memset(buffer, 0, HAHAFS_BLOCK_SIZE);
	memset(inode_buf, 0, 128);
	//TODO zero out disk

	//superblocks
	if (fseek(file, 0, SEEK_SET))
		goto file_error;
	if (!fwrite(&sb, sizeof(struct hahafs_super_block), 1, file))
		goto file_error;
	if (fseek(file, HAHAFS_BLOCK_SIZE * sb.snd_superblock_offset, SEEK_SET))
		goto file_error;
	if (!fwrite(&sb, sizeof(struct hahafs_super_block), 1, file))
		goto file_error;

	size_t inode_size = sizeof(struct hahafs_inode) + sb.file_name_len;
	uint32_t inodes_per_block = HAHAFS_BLOCK_SIZE / inode_size;
	uint32_t free_sectors = sectors_count - 2; //2 сектора на суперблоки
	// наборы из полностью заполненного сектора под иноды файлов и секторов под данные этих файлов
	uint32_t fully_filled =
		free_sectors / (1 + sb.file_sector_count * inodes_per_block);
	// оставшиеся сектора
	uint32_t remaining =
		free_sectors % (1 + sb.file_sector_count * inodes_per_block);

	// полные наборы + последний неполностью заполненный сектор под иноды файлов
	uint32_t files_count = fully_filled * inodes_per_block +
			       (remaining - 1) / sb.file_sector_count;

	if (fseek(file, HAHAFS_BLOCK_SIZE, SEEK_SET))
		goto file_error;
	for (uint32_t i = 0; i < files_count; i++) {
		struct hahafs_inode *inode = (struct hahafs_inode *)inode_buf;

		inode->size = 0;
		inode->hash = 0;
		sprintf(inode->name, "file%d", i);

		if (fseek(file,
			  HAHAFS_BLOCK_SIZE * (1 + i / inodes_per_block) +
				  inode_size * (i % inodes_per_block),
			  SEEK_SET))
			goto file_error;
		if (!fwrite(inode, inode_size, 1, file))
			goto file_error;
	}

	if (fseek(file, HAHAFS_BLOCK_SIZE * sectors_count - 1, SEEK_SET))
		goto file_error;

	fputc('\0', file);
	fclose(file);
	return 0;
file_error:
	fprintf(stderr, "file error: %s", strerror(errno));
	fclose(file);
	return 1;
}
