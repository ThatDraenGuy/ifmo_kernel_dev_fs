ROOT_DIR := $(shell pwd)
BUILD_DIR := $(ROOT_DIR)/build
WORK_DIR := $(ROOT_DIR)/work

MODULE_DIR := $(ROOT_DIR)/fs

SILENT_BUILD_FLAG := -s
TARGET_ARCH := x86_64
TARGET_GDB := gdb

KERNEL_VER ?= 6.12.74
KERNEL_DIR := linux-$(KERNEL_VER)
KERNEL_TAR := linux-$(KERNEL_VER).tar.xz

KERNEL_PATH := $(BUILD_DIR)/$(KERNEL_DIR)

NPROC := $(shell nproc)

MAKE := make -j $(NPROC) LLVM=1 LLVM_IAS=1 CC='ccache clang' -C $(KERNEL_PATH) ARCH=$(TARGET_ARCH)

$(WORK_DIR)/$(KERNEL_TAR):
	cd $(WORK_DIR)
	wget https://cdn.kernel.org/pub/linux/kernel/v6.x/$@
	cd -

$(KERNEL_PATH): $(WORK_DIR)/$(KERNEL_TAR)
	tar -xf $(WORK_DIR)/$(KERNEL_TAR) -C $(BUILD_DIR)

defconfig: $(KERNEL_PATH)
	$(MAKE) defconfig kvm_guest.config
	$(KERNEL_PATH)/scripts/config --enable DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT
	$(MAKE) olddefconfig

kernel_build: defconfig
# 	Enable reproducible builds for ccache
	export KBUILD_BUILD_TIMESTAMP=""
	$(MAKE) $(SILENT_BUILD_FLAG) all compile_commands.json
