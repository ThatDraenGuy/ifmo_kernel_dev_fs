
#include "../fs/src/fs.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#define BUF_SIZE 256

char buf[BUF_SIZE];

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "Usage: %s <fs path> <cmd> [args]\n", argv[0]);
		return 1;
	}

	char *fs_path = argv[1];
	char *cmd = argv[2];
	int fd = open(fs_path, O_RDONLY | O_DIRECTORY);

	if (fd == -1) {
		fprintf(stderr, "Error opening file\n");
		return 1;
	}

	if (strcmp(cmd, "clear") == 0) {
		if (ioctl(fd, IOC_CLEAN) != 0) {
			fprintf(stderr, "ioctl error\n");
			return 1;
		}
	} else if (strcmp(cmd, "erase") == 0) {
		if (ioctl(fd, IOC_ERASE) != 0) {
			fprintf(stderr, "ioctl error\n");
			return 1;
		}
	} else if (strcmp(cmd, "meta") == 0) {
		if (argc != 4) {
			fprintf(stderr,
				"Usage: %s <fs path> meta <file name>\n",
				argv[0]);
			return 1;
		}

		char *file_name = argv[3];

		strncpy(buf, file_name, BUF_SIZE);

		int ret = ioctl(fd, IOC_META, buf) != 0;
		if (ret != 0) {
			fprintf(stderr, "ioctl error %d\n", ret);
			return 1;
		}
		printf("%s\n", buf);
	} else if (strcmp(cmd, "sectors") == 0) {
		if (argc != 4) {
			fprintf(stderr,
				"Usage: %s <fs path> sectors <file name>\n",
				argv[0]);
			return 1;
		}

		char *file_name = argv[3];

		strncpy(buf, file_name, BUF_SIZE);

		int ret = ioctl(fd, IOC_SECTORS, buf) != 0;
		if (ret != 0) {
			fprintf(stderr, "ioctl error %d\n", ret);
			return 1;
		}
		printf("%s\n", buf);
	} else {
		fprintf(stderr, "Unknown command: %s\n", cmd);
		return 1;
	}
	return 0;
}