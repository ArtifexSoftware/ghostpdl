#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.

# $Id$

package="ghostscript"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoconf installed to compile $package."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	exit 1
}

echo "Generating configuration files for $package, please wait...."

echo "  creating links to configuration sources"
rm -f configure.ac Makefile.in
ln -s base/configure.ac .
ln -s base/Makefile.in .

echo "  running autoconf"
autoconf || exit 1

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
else
	echo "running ./configure $@"
fi

$srcdir/configure $@ && echo
