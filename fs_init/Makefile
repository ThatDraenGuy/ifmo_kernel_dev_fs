NAME = lab
CC = clang
CCFLAGS = -std=c99 -Wall -Werror -pedantic -g
DD = dd
DISK = ../work/image/disk.img

empty:
	dd if=/dev/zero of=$(DISK) bs=512 count=100

haha_init: main.c
	$(CC) $< -o $@

