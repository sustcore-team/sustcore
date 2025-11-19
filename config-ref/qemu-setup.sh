#!/bin/bash
#
# 仅支持某些发行版

set -e  # 遇到错误立即退出

echo -e "\\e[34mdownloading packages...\\e[0m"

echo "输入当前发行版的包管理器（仅输入序号）：
	1) apt
	2) pacman
	3) 其他"
read packman
case $packman in
	1) sudo apt install   libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build flex bison pip
	;;
	2) sudo pacman -S     libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build flex bison pip
	;;
	*) echo "暂不支持该发行版，请手动安装 libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build flex pip（软件包名称可能因发行版而不同）"
	;;
esac

export BUILD_THREADS=8
export QEMU=qemu-10.1.0
export PREFIX="$HOME/opt/cross"

curl https://download.qemu.org/$QEMU.tar.xz --output ./$QEMU.tar.xz

#解压
echo "extracting archives for qemu"
tar xf $QEMU.tar.xz

# 创建必要的目录
sudo mkdir -p "$PREFIX"

cd ./$QEMU/
	mkdir -p ./build/
	cd ./build/
		../configure --prefix="$PREFIX" --disable-werror
		make -j$BUILD_THREADS
		sudo make install
	cd ../
cd ../
