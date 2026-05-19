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
    OUTDIR=/tmp/aeld
else
    OUTDIR=$(realpath $1)
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}
if [ ! $? ]; then
    echo "Failed to create ${OUTDIR}!"
    exit 1
fi

cd $OUTDIR
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    #Clone only if the repository does not exist.
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout -b ${KERNEL_VERSION} ${KERNEL_VERSION}

    # DONE: Add your kernel build steps here
    # clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # config the defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # build
    make -j2 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # build kmodules
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    # build devicetree
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

# DONE: Create necessary base directories
mkdir rootfs
ROOTFS=${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr/{bin,lib,sbin} var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    #DONE:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# DONE: Make and install busybox
make -j2 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${ROOTFS} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a $ROOTFS/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a $ROOTFS/bin/busybox | grep "Shared library"

# DONE: Add library dependencies to rootfs
SYSROOT=$(realpath $(${CROSS_COMPILE}gcc -print-sysroot))
cp -a $SYSROOT/lib/ld-linux* $ROOTFS/lib/
cp -a $SYSROOT/lib64/libc.so.* $ROOTFS/lib64/
cp -a $SYSROOT/lib64/libm.so.* $ROOTFS/lib64/
cp -a $SYSROOT/lib64/libresolv.so.* $ROOTFS/lib64/

# DONE: Make device nodes
cd $ROOTFS
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# DONE: Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
# DONE: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
mkdir -p $ROOTFS/home/conf
cp $FINDER_APP_DIR/finder-test.sh $ROOTFS/home
cp $FINDER_APP_DIR/finder.sh $ROOTFS/home
cp $FINDER_APP_DIR/writer $ROOTFS/home
cp $FINDER_APP_DIR/conf/username.txt $ROOTFS/home/conf
cp $FINDER_APP_DIR/conf/assignment.txt $ROOTFS/home/conf
cp $FINDER_APP_DIR/autorun-qemu.sh $ROOTFS/home


# DONE: Chown the root directory
sudo chown -R root:root $ROOTFS/*

# DONE: Create initramfs.cpio.gz
cd $ROOTFS
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
