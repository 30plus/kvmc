#!/bin/sh
########################################################################
# Update with up-to-date public header files from a Linux source tree.
# If no directory is given, use the lib/modules/`uname -r`/source link.
########################################################################
if [ "$#" -ge 1 ]
then
	LINUX_ROOT="$1"
else
	LINUX_ROOT=/lib/modules/$(uname -r)/source
fi

if [ ! -d $LINUX_ROOT/include/uapi/linux ]
then
	echo "$LINUX_ROOT does not seem to be valid Linux source tree."
	echo "usage: $0 [path-to-Linux-source-tree]"
	exit 1
fi

cp $LINUX_ROOT/include/uapi/linux/kvm.h include/linux

for arch in arm64 mips powerpc x86
do
	case "$arch" in
		arm64) ARCH_PATH=arm/aarch64 ;;
		*) ARCH_PATH=$arch ;;
	esac
	cp $LINUX_ROOT/arch/$arch/include/uapi/asm/kvm.h \
		$ARCH_PATH/include/asm
done
