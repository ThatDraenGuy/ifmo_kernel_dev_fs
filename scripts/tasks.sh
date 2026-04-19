#!/bin/bash

function depend_on() {
  $SCRIPT $@
  if [[ "$CLEAR" == 1 ]]; then
    clear
  fi
}

function spinner() {
  local pid=$1

  if [[ "$SPINNER" -eq 1 ]]; then
    local spin='⣾⣽⣻⢿⡿⣟⣯⣷'
    local i=0
    tput civis # Hide cursor
    while kill -0 $pid 2>/dev/null; do
      local i=$(((i + 1) % ${#spin}))
      printf "%s" "${spin:$i:1}" # Print one character
      echo -en "\033[$1D" # Go back one character
      sleep .1
    done
    tput cnorm # Restore cursor
  fi

  wait $pid
  return $?
}

set -e

# Arguments extraction
if [ "$#" -lt 1 ]; then
  echo "Usage: $0 command"
  exit 1
fi
COMMAND=$1

# See https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html
# for the `: ${var:=DEFAULT}` syntax
: ${SCRIPT:=`realpath -s "$0"`}
: ${SCRIPT_DIR:=`dirname "${SCRIPT}"`}

# Let the user override environment variables for their special needs
files_to_source=$(find ${SCRIPT_DIR}/ -maxdepth 1 -xtype f -name "local*.sh")
for file in $files_to_source; do
  source "$file"
done

# Default context variables, can be overridden by local.sh or in environment.
: ${ROOT_DIR:=`realpath -s "${SCRIPT_DIR}/.."`}
: ${BUILD_DIR:="${ROOT_DIR}/build"}
: ${WORK_DIR:="${ROOT_DIR}/work"}
: ${KERNEL_VER:="6.12.74"}
: ${KERNEL_DIR:="${BUILD_DIR}/linux-${KERNEL_VER}"}
: ${MAKE:="make -j`nproc` LLVM=1 LLVM_IAS=1 CC='ccache clang'"}
: ${TARGET_ARCH:="x86_64"}
: ${TARGET_GDB:="gdb"}
: ${SILENT_BUILD_FLAG="-s"}
: ${SUCCESSFUL_EXIT_COMMAND:=""}
: ${BPF_SELFTESTS_DIR:="${KERNEL_DIR}/tools/testing/selftests/bpf"}
: ${VM_START_ARGS:=''}
: ${SYZ_MANAGER_CFG_EXTRA:=''}
: ${KERNEL_CMDLINE_EXTRA:=''}
: ${SPINNER:=1}
: ${IMAGE_DIR:="${WORK_DIR}/image"}
: ${IMAGE_PATH:="${IMAGE_DIR}/debian-${TARGET_ARCH}.img"}
: ${TRACER_PATH:="${SCRIPT_DIR}/autostart/tracer.stp"}
if [[ "$TERM_PROGRAM" == "vscode" ]]; then
  : ${CLEAR:=1}
fi
if [[ $SKIP_SYSTEMD == 1 ]]; then
  KERNEL_CMDLINE_EXTRA="init=/sbin/init-minimal $KERNEL_CMDLINE_EXTRA"
fi

# Convenience environment variables derived from the context
if [ "${TARGET_ARCH}" = "x86_64" ]; then
  : ${VMLINUX:="bzImage"}
  : ${CLANG_TARGET:="x86_64-linux-gnu"}
  : ${MKOSI_TARGET_ARCH:="x86-64"}
  : ${TOOLS_SRCARCH:="x86"}
  : ${QEMU_BIN:="qemu-system-x86_64"}
  : ${QEMU_CMD:="${QEMU_BIN} -enable-kvm -cpu host -machine q35 -bios qboot.rom"}
  : ${SERIAL_TTY:="ttyS0"}
  : ${ROOT_MNT:="/dev/sda"}
elif [ "${TARGET_ARCH}" = "arm64" ]; then
  : ${VMLINUX:="Image"}
  : ${CLANG_TARGET:="aarch64-linux-gnu"}
  : ${MKOSI_TARGET_ARCH:="arm64"}
  : ${TOOLS_SRCARCH:="arm64"}
  : ${QEMU_BIN:="qemu-system-aarch64"}
  : ${QEMU_CMD:="${QEMU_BIN} -cpu max -machine virt"}
  : ${SERIAL_TTY:="ttyAMA0"}
  : ${PROOT_ARGS:="-q qemu-aarch64-static"}
  : ${ROOT_MNT:="/dev/vda"}
else
  echo "Unsupported TARGET_ARCH:" $TARGET_ARCH
  exit 2
fi

: ${KERNEL_PATH:="${KERNEL_DIR}/arch/${TARGET_ARCH}/boot/${VMLINUX}"}


if [ ! -d ${KERNEL_DIR} ]; then
    mkdir -p ${BUILD_DIR}

    if [ ! -f ${WORK_DIR}/linux-${KERNEL_VER}.tar.xz ]; then
      cd ${WORK_DIR}
      wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VER}.tar.xz
      cd ${ROOT_DIR}
    fi

    tar -xf ${WORK_DIR}/linux-${KERNEL_VER}.tar.xz -C ${BUILD_DIR}
fi

# When called outside of a VSCode task, the current working directory can be
# somewhere else than the workspace. Since we implicitly rely on pwd being the
# top of the kernel tree quite often, cd there.
pushd "$KERNEL_DIR" >/dev/null

if [[ "$CLEAR" == 1 ]]; then
  clear
fi

# SSH Keys
: ${SSH_KEY:="${WORK_DIR}/linux-kernel-vscode-rsa"}
: ${SSH_CMD:="ssh -p 5555 -i ${SSH_KEY} -o IdentitiesOnly=yes -o NoHostAuthenticationForLocalhost=yes root@localhost"}
: ${SCP_CMD:="scp -P 5555 -r -i ${SSH_KEY} -o IdentitiesOnly=yes -o NoHostAuthenticationForLocalhost=yes"}
mkdir -p ${WORK_DIR}
if [ ! -f ${SSH_KEY} ]; then
  ssh-keygen -t rsa -f ${SSH_KEY} -N "" -q
fi

# QEMU start command
: ${VM_START:="${QEMU_CMD} -s -nographic -smp 4 -m 4G -qmp tcp:localhost:4444,server,nowait -serial mon:stdio \
    -net nic,model=virtio-net-pci -net user,hostfwd=tcp::5555-:22 \
    -virtfs local,path=/,mount_tag=hostfs,security_model=none,multidevs=remap \
    -append \"console=${SERIAL_TTY},115200 root=${ROOT_MNT} rw nokaslr init=/lib/systemd/systemd debug systemd.log_level=info ${KERNEL_CMDLINE_EXTRA}\" \
    -drive file=${IMAGE_PATH},format=raw -kernel ${KERNEL_PATH} ${VM_START_ARGS}"}

case "${COMMAND}" in
# Virtual machine life-cycle
  "start")
    depend_on install-autostart
    eval ${VM_START}
    ;;
  "start-wait-dbg")
    depend_on install-autostart
    eval ${VM_START} -S
    ;;
  "stop")
    # With SKIP_SYSTEMD, nothing handles ACPI shutdowns so clean shutdown does not work.
    if [[ -z $SKIP_SYSTEMD ]]; then
      echo -n '{"execute":"qmp_capabilities"} {"execute": "system_powerdown"}' | nc -w 1 localhost 4444
    else
      killall ${QEMU_BIN}
    fi
    ;;
  "ssh")
    eval ${SSH_CMD}
    ;;
  "run")
    shift
    eval ${SSH_CMD} $@
    ;;
  "wait-for-vm")
    # On the first boot, a rootfs isn't yet available. Because debootstrap can
    # take a while to run, this waits for the rootfs file to show up.
    timeout 120 bash -c "until [ -f ${IMAGE_PATH} ] ; do sleep 0.01; done"
    ;;
# Kernel build
  "defconfig")
    # Only generate .config if it doesn't already exist
    if [ ! -f ${KERNEL_DIR}/.config ]; then
      eval ${MAKE} ARCH=${TARGET_ARCH} defconfig kvm_guest.config
      scripts/config --enable DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT
      eval ${MAKE} ARCH=${TARGET_ARCH} olddefconfig
    fi
    ;;
  "menuconfig")
    # It's important to run menuconfigs with the same parameters as builds
    eval ${MAKE} ARCH=${TARGET_ARCH} menuconfig
    ;;
  "clean")
    eval ${MAKE} ARCH=${TARGET_ARCH} clean
    ;;
  "build")
    depend_on defconfig

    # Enable reproducible builds for ccache
    export KBUILD_BUILD_TIMESTAMP=""
    # Generate not only the kernel but also the clangd config
    CMD="${MAKE} ${SILENT_BUILD_FLAG} ARCH=${TARGET_ARCH} all compile_commands.json"
    echo ${CMD}
    eval ${CMD} &
    spinner $!

    # A gdb index may need to be re-generated. Don't clear the above make logs.
    CLEAR=0 $SCRIPT gdb-index
    # A tracer module may need to be re-built
    CLEAR=0 $SCRIPT systemtap-build
    ;;
  "gdb-index")
    # Hitting a breakpoint is *much* faster if we pre-build a gdb symbol index
    if ! readelf -S vmlinux | grep -q ".gdb_index"; then
      OBJCOPY=llvm-objcopy GDB=${TARGET_GDB} gdb-add-index vmlinux
    fi
    ;;
# Rootfs management
  "create-rootfs")
    # Only generate a rootfs if it doesn't already exist
    if [ ! -f ${IMAGE_PATH} ]; then
      img="$(mktemp -u --suffix=.img)"
      img_mnt="$(mktemp -d)"
      img_bind_mnt="$(mktemp -d)"
      trap 'rm -f ${img}; sudo umount -l ${img_bind_mnt}; sudo umount -l ${img_mnt}; rmdir ${img_mnt} ${img_bind_mnt}' ERR
      # Image file creation
      qemu-img create ${img} 20G
      mkfs -t ext4 ${img}

      # Mounts (bind mounts for permission)
      mkdir -p ${img_mnt} ${img_bind_mnt}
      echo "password required to mount the rootfs:"
      sudo mount -o loop ${img} ${img_mnt}
      sudo bindfs --uid-offset=$(id -u) --gid-offset=$(id -g) \
          --create-with-perms=0644,ud+X:gd-rwX:od-rwX ${img_mnt} ${img_bind_mnt}

      # Debian rootfs generation and config setting
      sudo mkosi --architecture=${MKOSI_TARGET_ARCH} --distribution=debian --release=unstable --output-dir=${img_mnt} --format=directory \
      --package=ssh,acpid,acpi-support-base,gdb,systemtap,file,psmisc,strace,vim,bpftool,bpftrace,trace-cmd,linux-perf \
      --package=apt,less,login,iputils-ping,iproute2,cron,e2fsprogs,systemd-sysv,cpio,dhcpd,fdisk,udev,man

      # Move mkosi-generated rootfs from ${img_mnt}/image to ${img_mnt} to match script's expected directory structure
      sudo mv ${img_mnt}/image/* ${img_mnt} && sudo rmdir ${img_mnt}/image

      echo "debian-vm" > ${img_bind_mnt}/etc/hostname
      echo "nameserver 8.8.8.8" > ${img_bind_mnt}/etc/resolv.conf
      echo "hostfs /host 9p trans=virtio,rw,nofail 0 0" > ${img_bind_mnt}/etc/fstab
      printf "[Match]\nName=en*\n[Network]\nDHCP=yes" > ${img_bind_mnt}/etc/systemd/network/80-dhcp.network
      sed -i 's~^ExecStart=.*~ExecStart=-/sbin/agetty --autologin root -o "-p -f root" --keep-baud 115200,57600,38400,9600 - $TERM~' ${img_bind_mnt}/lib/systemd/system/serial-getty@.service
      mkdir -p ${img_bind_mnt}/root/.ssh/
      cp ${SSH_KEY}.pub ${img_bind_mnt}/root/.ssh/authorized_keys
      sudo chroot ${img_mnt} systemctl enable systemd-networkd acpid
      cat << EOF > ${img_bind_mnt}/sbin/init-minimal
#!/bin/sh

# Mount various important file systems
mkdir -p /proc /sys /run/ /tmp /dev
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /run
mount -t tmpfs none /tmp
mount -t devtmpfs none /dev
mkdir -p /dev/pts
mount -t devpts none /dev/pts
# And the content of /etc/fstab
mount -a

# Set the network interface up
cat /etc/hostname > /proc/sys/kernel/hostname
ip link set eth0 up
dhclient eth0

# Change cwd to /root
HOME=/root
cd $HOME

# Start the SSH server
mkdir /run/sshd/
/usr/sbin/sshd

# Start the autostart script if there is one
[ -f /usr/bin/autostart.sh ] && /usr/bin/autostart.sh &

# Set up the serial line and get to a bash prompt
setsid /sbin/getty -l /bin/bash -n 115200 ${SERIAL_TTY}
EOF
      chmod +x ${img_bind_mnt}/sbin/init-minimal
      cat << EOF > ${img_bind_mnt}/etc/bash.bashrc
# Use a green or red prompt depending on the previous command's exit value
__prompt () {
   if [[ $? = "0" ]]; then
     PS1='\[\033[01;32m\]'
   else
     PS1='\[\033[01;31m\]'
  fi
  PS1="$PS1\u@\h:\w\$ \[\033[00m\]"
}
PROMPT_COMMAND=__prompt

# On the serial tty, ask the host terminal for dimensions before each command
__resize () {
  local escape r c
  IFS='[;' read -t 1 -sd R -p "$(printf '\e7\e[r\e[999;999H\e[6n\e8')" escape r c
  if [[ "$r" -gt 0 && "$c" -gt 0 ]]; then
    stty cols $c rows $r
  fi
}
if [[ $TERM = "vt102" ]]; then
  trap __resize DEBUG
fi
EOF

      # Atomically make the rootfs file available to unblock wait-for-vm tasks
      sync
      sudo umount -l ${img_bind_mnt}
      sudo umount -l ${img_mnt}
      mkdir -p "${IMAGE_DIR}"
      mv "${img}" "${IMAGE_PATH}"
    fi
    ;;
  "install-autostart")
    depend_on create-rootfs

    cd ${SCRIPT_DIR}/autostart/
    BUILT_AUTOSTART=${IMAGE_DIR}/autostart-${TARGET_ARCH}

    # The poor man's make. We use the last built /tmp/autostart to track if any
    # of the source file has changed. Only if one changed, rebuild and install.
    if [ ${BUILT_AUTOSTART} -nt autostart.c ] && \
       [ ${BUILT_AUTOSTART} -nt autostart.sh ] &&
       [ ${BUILT_AUTOSTART} -nt autostart.service ]; then
      echo "Autostart already up to date"
      exit 0
    fi

    clang --target=${CLANG_TARGET} -fuse-ld=lld `cat compile_flags.txt` autostart.c -o ${BUILT_AUTOSTART}

    echo Installing autostart on `basename ${IMAGE_PATH}`
    guestfish --rw -a "${IMAGE_PATH}" << EOF
      run
      mount /dev/sda /

      upload ${BUILT_AUTOSTART} /usr/bin/autostart
      chmod 755 /usr/bin/autostart

      upload autostart.sh /usr/bin/autostart.sh
      chmod 755 /usr/bin/autostart.sh

      upload autostart.service /lib/systemd/system/autostart.service
      ln-sf /lib/systemd/system/autostart.sh /etc/systemd/system/multi-user.target.wants/autostart.service
EOF
    ;;
  "push")
    if [ "$#" -lt 2 ]; then
      echo "Usage: $0 push /file/to/push [/destination]"
      exit 1
    fi
    popd >/dev/null
    eval ${SCP_CMD} ${2} root@localhost:${3:-/root}
    ;;
  "pull")
    if [ "$#" -lt 2 ]; then
      echo "Usage: $0 pull /file/to/pull [/destination]"
      exit 1
    fi
    popd >/dev/null
    eval ${SCP_CMD} root@localhost:${2} ${3:-.}
    ;;
  "chroot")
      img_mnt="$(mktemp -d)"
      echo "password required to mount the rootfs:"
      sudo mount -o loop ${IMAGE_PATH} ${img_mnt}
      trap 'sudo umount -l ${img_mnt}; rmdir ${img_mnt}' EXIT
      sudo proot -S ${img_mnt} -w / ${PROOT_ARGS}
    ;;
# Tracing
  "systemtap-build")
    if [ -f ${TRACER_PATH} ]; then
      echo Re-building ${TRACER_PATH} ...
      # Workaround the presence of mcount nops with PR15123_ASSUME_MFENTRY
      # Skip the loading part of the pipeline with -p4. Use Guru mode with -g
      # Workaround clang warnings (treated as errors) with -Wno-everything
      PR15123_ASSUME_MFENTRY=1 stap -p4 -g -r ${KERNEL_DIR} -m tracer \
        -B LLVM=1 -B CFLAGS_MODULE="-Wno-everything" ${TRACER_PATH} > /dev/null
      # The guest doesn't know $KERNEL_DIR but it can hardcode /host/tmp/
      echo Installing to /tmp/tracer.ko ...
      mv tracer.ko /tmp/
    else
      rm -f /tmp/tracer.ko
    fi
    ;;
  *)
    echo "Invalid command"
    ;;
esac
