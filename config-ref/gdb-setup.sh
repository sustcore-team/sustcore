#!/bin/bash
#
# 仅支持某些发行版

set -e  # 遇到错误立即退出

echo -e "\\e[34mdownloading packages...\\e[0m"

export GDB=gdb-16.3

curl https://mirrors.aliyun.com/gnu/gdb/$GDB.tar.xz --output ./$GDB.tar.xz	

#解压

echo "extracting archives for gdb"
tar xf $GDB.tar.xz

export BUILD_THREADS=8
export PREFIX="$HOME/opt/cross"
export TARGET1=x86_64-elf
export TARGET2=riscv64-unknown-elf
export TARGET3=loongarch64-unknown-elf
export PATH="$PREFIX/bin:$PATH"

# 创建必要的目录
sudo mkdir -p "$PREFIX"

# 为x86-64编译GDB

echo -e "\\e[34mbuilding gdb for $TARGET1\\e[0m"
cd ./$GDB/
	mkdir -p ./build/$TARGET1/
	cd ./build/$TARGET1/
		../configure --target=$TARGET1 --prefix="$PREFIX" --disable-werror
        make all-gdb -j$BUILD_THREADS
        make install-gdb
	cd ../../
cd ../

# 为riscv编译GDB

echo -e "\\e[34mbuilding gdb for $TARGET2\\e[0m"
cd ./$GDB/
	mkdir -p ./build/$TARGET2/
	cd ./build/$TARGET2/
		../configure --target=$TARGET2 --prefix="$PREFIX" --disable-werror
        make all-gdb -j$BUILD_THREADS
        make install-gdb
	cd ../../
cd ../

# 为LOONGARCH编译GDB

echo -e "\\e[34mbuilding gdb for $TARGET3\\e[0m"
cd ./$GDB/
	mkdir -p ./build/$TARGET3/
	cd ./build/$TARGET3/
		../configure --target=$TARGET3 --prefix="$PREFIX" --disable-werror
        make all-gdb -j$BUILD_THREADS
        make install-gdb
	cd ../../
cd ../