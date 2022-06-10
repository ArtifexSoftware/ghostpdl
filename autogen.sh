#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.
#
# NOCONFIGURE
#  If set to any value it will generate all files but not invoke the
#  generated configure script.
#   e.g. NOCONFIGURE=1 ./autogen.sh

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

(automake --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have automake installed to compile $package."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
        exit 1
}

rm -rf autom4te.cache

echo "Generating configuration files for $package, please wait...."

if test ! -x config.guess -o ! -x config.sub ; then
  rm -f config.guess config.sub
  cp `automake --print-libdir`/config.guess . || exit 1
  cp `automake --print-libdir`/config.sub . || exit 1
fi

if test ! -x install-sh ; then
  rm -f install-sh
  cp `automake --print-libdir`/install-sh . || exit 1
fi

echo "  running autoreconf"
autoreconf || exit 1


if test x"$NOCONFIGURE" = x""; then
  if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
  else
	echo "running ./configure $@"
  fi

  $srcdir/configure "$@" && echo
fi
