CC = clang
CCFLAGS = -std=c99 -Wall -Werror -pedantic -g
DD = dd
DISK = ../work/image/disk.img

default_disk: haha_init empty
	./haha_init $(DISK) 1000 64 16 10

haha_init: main.c
	$(CC) $< -o $@

empty:
	dd if=/dev/zero of=$(DISK) bs=512 count=100

.PHONY: empty default_disk

