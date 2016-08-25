#!/bin/bash
#

if [ $# -gt 1 ]; then
  echo "Usage: build_ios_gslib.sh [debug]"
  exit
fi

if [ $# -eq 1 ]; then
  if test x"$1" = x"debug"; then
    DEBUGSTR=debug
  fi
fi

startdir="$PWD"

cd ..

if [ ! -x ./configure ] ; then
  ./autogen.sh --help
fi

export CC="$(xcrun --sdk iphonesimulator --find cc)"
export CPP="$(xcrun --sdk iphonesimulator --find cpp)"
export RANLIB="$(xcrun --sdk iphonesimulator --find ranlib)"
export CFLAGS=" -isysroot $(xcrun --sdk iphonesimulator --show-sdk-path) -Wno-implicit-function-declaration -arch x86_64 -arch i386"

export BIGENDIAN=0
export CCAUX=/usr/bin/gcc
export CFLAGSAUX=" "

./configure --without-x --with-arch_h=./ios/ios_arch-x86.h --host=x86_64-apple-darwin7 --build=x86_64-linux-gnu 2>&1 | tee conflog_x86.txt || exit 1

make -j4 BUILDDIRPREFIX=ios_x86- GS=libgs_x86 libgs$DEBUGSTR 2>&1 | tee buildlog_x86.txt || exit 1

mv Makefile Makefile.x86

export CC="$(xcrun --sdk iphoneos --find cc)"
export CPP="$(xcrun --sdk iphoneos --find cpp)"
export CFLAGS=" -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -Wno-implicit-function-declaration -arch armv7 -arch armv7s -arch arm64"
export RANLIB="$(xcrun --sdk iphoneos --find ranlib)"

./configure --without-x  --with-arch_h=./ios/ios_arch-arm.h --host=armv7-apple-darwin7 --build=x86_64-linux-gnu 2>&1 | tee conflog_arm.txt || exit 1

make -j4 BUILDDIRPREFIX=ios_arm- GS=libgs_arm libgs$DEBUGSTR 2>&1 | tee buildlog_arm.txt || exit 1

mv Makefile Makefile.arm

cd $startdir

export LIPO="$(xcrun --sdk iphoneos --find lipo)"

xcrun lipo -output libgs.a -create ../ios_x86-"$DEBUGSTR"bin/libgs_x86.a ../ios_arm-"$DEBUGSTR"bin/libgs_arm.a 2>&1 | tee buildlog_uni.txt || exit 1

