#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # 1. 编译内核配置与镜像
    echo "Configuring the Linux kernel for ${ARCH}"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "Building the Linux kernel Image"
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "Building device tree blobs"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
# 2. 创建基础根文件系统目录骨架
mkdir -p ${OUTDIR}/rootfs
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    # 3. 配置 Busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
# 4. 编译并安装 Busybox 到 rootfs
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

# 切换到 rootfs 目录以便 readelf 寻找 bin/busybox
cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

# TODO: Make device nodes

# TODO: Clean and build the writer utility

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

# TODO: Chown the root directory

# TODO: Create initramfs.cpio.gz

# 5. 复制动态链接依赖库至 rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

# 动态链接器（程序解释器）复制到 /lib
cp -L ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/ 2>/dev/null || cp -L ${SYSROOT}/lib64/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/

# 共享库复制到 /lib64 (使用 -L 解除软链接引用，防止断链)
cp -L ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/ 2>/dev/null || cp -L ${SYSROOT}/lib/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp -L ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/ 2>/dev/null || cp -L ${SYSROOT}/lib/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
cp -L ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/ 2>/dev/null || cp -L ${SYSROOT}/lib/libc.so.6 ${OUTDIR}/rootfs/lib64/

# 6. 创建字符设备节点
sudo rm -f ${OUTDIR}/rootfs/dev/null ${OUTDIR}/rootfs/dev/console
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1

# 7. 交叉编译 writer 实用工具
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# 8. 拷贝作业脚本和可执行程序至 /home 目录
mkdir -p ${OUTDIR}/rootfs/home/conf
cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf/
cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/conf/

# 修正 finder-test.sh 中的路径引用（平铺在 /home 下不再需要 ../）
sed -i 's|\.\./conf/assignment\.txt|conf/assignment\.txt|g' ${OUTDIR}/rootfs/home/finder-test.sh

# 9. 递归修改 rootfs 属主为 root
cd ${OUTDIR}/rootfs
sudo chown -R root:root ${OUTDIR}/rootfs

# 10. 创建 initramfs.cpio.gz 独立内存盘镜像
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio

echo "Rootfs and Kernel build complete! Output at: ${OUTDIR}"
