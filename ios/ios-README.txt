To build a combined armv7/arm64/i386/x86_64 Ghostscript static library for ios,
in the ghostpdl/ios directory, run the "build_ios_gslib.sh".

That script will run autogen.sh if required (if configure does not already
exist), then call configure with the required options, and then make, first
to build for x86_64/i386 then for armv7/arm64.

The two separate builds end up in ghostpdl/ios_x86-bin and ios_arm-bin
respectively.

The script then uses the lipo utility to combine the two static library
files into a single "fat" binary, which lands in ghostpdl/ios/libgs.a
