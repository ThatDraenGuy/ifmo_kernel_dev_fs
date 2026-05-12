
ROOT_DIR := $(abspath $(PWD)/..)
KERNEL_DIR := $(ROOT_DIR)/build/linux-6.12.74
MAKE := make -j`nproc` LLVM=1 LLVM_IAS=1 CC='ccache clang' -C $(KERNEL_DIR)

MODULE_NAME := haha_fs
$(MODULE_NAME)-y := src/main.o src/fs.o src/inode.c src/superblock.o
obj-m := $(MODULE_NAME).o

BUILD_DIR := build
BUILD_PATH := $(PWD)/$(BUILD_DIR)

$(BUILD_PATH):
	mkdir -p "$@"

all: $(BUILD_PATH)
	rsync -a --exclude=$(BUILD_DIR) ./ $(BUILD_PATH)
	cd $(BUILD_PATH)
	$(MAKE) M=$(BUILD_PATH) modules compile_commands.json
	cd -

modules_install:
	$(MAKE) M=$(BUILD_PATH) src=$(PWD) modules_install

clean:
	rm -Rf $(BUILD_PATH)
