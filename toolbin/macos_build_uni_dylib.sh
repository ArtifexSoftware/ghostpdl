#!/bin/bash
#

if [ ! -f base/version.mak ] ; then
  echo "This script expects to be run from the ghostpdl tree root"
  exit
fi

if [ $# -gt 1 ]; then
  echo "Usage: build_ios_gslib.sh"
  exit
fi

DEBUGSTR=
if [ $# -eq 1 ]; then
  if test x"$1" = x"debug"; then
    DEBUGSTR=debug
  fi
fi

GS_VERSION_MAJOR=$(grep "GS_VERSION_MAJOR="  base/version.mak | awk '{ split($0, arr, "="); print arr[2]; }')
GS_VERSION_MINOR=$(grep "GS_VERSION_MINOR="  base/version.mak | awk '{ split($0, arr, "="); print arr[2]; }')


if [ ! -x ./configure ] ; then
  NOCONFIGURE=1
  export NOCONFIGURE
  ./autogen.sh
  NOCONFIGURE=
  export NOCONFIGURE
fi


this_host=$(./config.guess)

native_arch=""
case $this_host in
    *x86_64*)
      native_arch=x86_64
    ;;
    *aarch64*)
      native_arch=arm64
    ;;
esac

if test x"$native_arch" = x"" ; then
    echo "Error: Unable to identify native architecture"
    exit 1
fi

./configure CC="gcc -arch x86_64" CXX="g++ -arch x86_64" CCAUX="gcc -arch $native_arch" --without-x --host=$this_host --build=x86_64-apple-darwin --with-arch_h="arch/osx-x86-x86_64-ppc_arm64-gcc.h" 2>&1 | tee conflog_x86_64.txt || exit 1

make -j4 so 2>&1 | tee buildlog_x86_64.txt || exit 1

mv Makefile Makefile.x86_64
mv sobin sobin-x86_64
mv soobj soobj-x86_64

./configure CC="gcc -arch arm64" CXX="g++ -arch arm64" CCAUX="gcc -arch $native_arch" --without-x --host=$this_host --build=arm64-apple-darwin --with-arch_h="arch/osx-x86-x86_64-ppc_arm64-gcc.h" 2>&1 | tee conflog_arm64.txt || exit 1

make -j4 so 2>&1 | tee buildlog_arm64.txt || exit 1
mv sobin sobin-arm64
mv soobj soobj-arm64

mv Makefile Makefile.arm64


mkdir sobin

lipo -output sobin/libgs.$GS_VERSION_MAJOR.$GS_VERSION_MINOR.dylib -create sobin-x86_64/libgs.$GS_VERSION_MAJOR.$GS_VERSION_MINOR.dylib sobin-arm64/libgs.$GS_VERSION_MAJOR.$GS_VERSION_MINOR.dylib 2>&1 | tee buildlog_uni.txt || exit 1
lipo -output sobin/gsc -create sobin-x86_64/gsc sobin-arm64/gsc 2>&1 | tee buildlog_uni.txt || exit 1

cd sobin

ln -s libgs.$GS_VERSION_MAJOR.$GS_VERSION_MINOR.dylib libgs.$GS_VERSION_MAJOR.dylib
ln -s libgs.$GS_VERSION_MAJOR.$GS_VERSION_MINOR.dylib libgs.dylib
cd ..

rm -rf sobin-x86_64 soobj-x86_64 sobin-arm64 soobj-arm64

rm Makefile.x86_64 Makefile.arm64

exit 0
