#!/bin/bash
#
# 仅支持某些发行版

set -e  # 遇到错误立即退出

echo "输入当前发行版的包管理器（仅输入序号）：
	1) apt
	2) pacman
	3) 其他"
read packman
case $packman in
	1) sudo apt install   make gcc g++ ninja-build libsdl2-dev texinfo curl wget git m4 xz-utils
	;;
	2) sudo pacman -S     make gcc g++ ninja-build libsdl2-dev texinfo curl wget git m4 xz-utils
	;;
	*) echo "暂不支持该发行版，请手动安装gcc g++ ninja-build libsdl2-dev texinfo curl wget git m4 xz-utils（软件包名称可能因发行版而不同）"
	;;
esac

echo -e "\\e[34mdownloading packages...\\e[0m"

export BUILD_THREADS=8
export MPFR=mpfr-4.1.0
export MPC=mpc-1.2.1
export GMP=gmp-6.2.1
export BINUTILS=binutils-2.44
export GCCV=gcc-15.2.0

curl https://mirrors.aliyun.com/gnu/mpfr/$MPFR.tar.xz --output ./$MPFR.tar.xz
curl https://mirrors.aliyun.com/gnu/mpc/$MPC.tar.gz --output ./$MPC.tar.gz
curl https://mirrors.aliyun.com/gnu/gmp/$GMP.tar.xz --output ./$GMP.tar.xz
curl https://mirrors.aliyun.com/gnu/binutils/$BINUTILS.tar.xz --output ./$BINUTILS.tar.xz
curl https://mirrors.aliyun.com/gnu/gcc/$GCCV/$GCCV.tar.xz --output ./$GCCV.tar.xz	

#解压
echo "extracting archives for mpfr"
tar xf $MPFR.tar.xz
echo "extracting archives for mpc"
tar xf $MPC.tar.gz

echo "extracting archives for gmp"
tar xf $GMP.tar.xz

echo "extracting archives for binutils"
tar xf $BINUTILS.tar.xz

echo "extracting archives for gcc"
tar xf $GCCV.tar.xz

export PREFIX="$HOME/opt/cross"
export TARGET1=x86_64-elf
export TARGET2=riscv64-unknown-elf
export TARGET3=loongarch64-unknown-elf
export PATH="$PREFIX/bin:$PATH"

# 创建必要的目录
sudo mkdir -p "$PREFIX"

#编译GMP, MPFR与MPC

echo -e "\\e[34mbuilding gmp\\e[0m"
cd ./$GMP/
	mkdir -p ./build/
	cd ./build/
		../configure
		make -j$BUILD_THREADS
		sudo make install
	cd ../
cd ../

echo -e "\\e[34mbuilding mpfr\\e[0m"
cd ./$MPFR/
	mkdir -p ./build/
	cd ./build/
		../configure
		make -j$BUILD_THREADS
		sudo make install
	cd ../
cd ../

echo -e "\\e[34mbuilding mpc\\e[0m"
cd ./$MPC/
	mkdir -p ./build/
	cd ./build/
		../configure
		make -j$BUILD_THREADS
		sudo make install
	cd ../
cd ../

# 为X86-64编译GCC

echo -e "\\e[34mbuilding binutils for $TARGET1\\e[0m"
cd ./$BINUTILS/
	mkdir -p ./build/$TARGET1/
	cd ./build/$TARGET1/
		../../configure --target=$TARGET1 --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
		make -j$BUILD_THREADS
		sudo make install
	cd ../../
cd ../

which -- $TARGET1-as || echo $TARGET1-as is not in the PATH

echo -e "\\e[34mbuilding gcc\\e[0m"
cd ./$GCCV/
    mkdir -p ./build/$TARGET1/
    cd ./build/$TARGET1/
        ../../configure --target=$TARGET1 --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
        make all-gcc -j$BUILD_THREADS
        make all-target-libgcc -j$BUILD_THREADS
        sudo make install-gcc
        sudo make install-target-libgcc
    cd ../../
cd ../

# 为RISCV编译GCC

echo -e "\\e[34mbuilding binutils for $TARGET2\\e[0m"
cd ./$BINUTILS/
	mkdir -p ./build/$TARGET2/
	cd ./build/$TARGET2/
		../../configure --target=$TARGET2 --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror 
		make -j$BUILD_THREADS 
		sudo make install 
	cd ../../
cd ../

which -- $TARGET2-as || echo $TARGET2-as is not in the PATH

echo -e "\\e[34mbuilding gcc\\e[0m"
cd ./$GCCV/
    mkdir -p ./build/$TARGET2/
    cd ./build/$TARGET2/
        ../../configure --target=$TARGET2 --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers 
        make all-gcc -j$BUILD_THREADS 
        make all-target-libgcc -j$BUILD_THREADS 
        sudo make install-gcc 
        sudo make install-target-libgcc 
    cd ../../
cd ../

# 为LOONGARCH编译GCC

echo -e "\\e[34mbuilding binutils for $TARGET3\\e[0m"
cd ./$BINUTILS/
	mkdir -p ./build/$TARGET3/
	cd ./build/$TARGET3/
		../../configure --target=$TARGET3 --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
		make -j$BUILD_THREADS 
		sudo make install 
	cd ../../
cd ../

which -- $TARGET3-as || echo $TARGET3-as is not in the PATH

echo -e "\\e[34mbuilding gcc\\e[0m"
cd ./$GCCV/
    mkdir -p ./build/$TARGET3/
    cd ./build/$TARGET3/
        ../../configure --target=$TARGET3 --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
        make all-gcc -j$BUILD_THREADS
        make all-target-libgcc -j$BUILD_THREADS
        sudo make install-gcc
        sudo make install-target-libgcc
    cd ../../
cd ../