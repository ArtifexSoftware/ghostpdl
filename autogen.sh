#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.

# $Id: autogen.sh,v 1.2 2002/07/08 13:40:15 giles Exp $

package="jbig2dec"
AUTOMAKE_FLAGS="--foreign $AUTOMAKE_FLAGS"

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

echo "  aclocal $ACLOCAL_FLAGS"
aclocal $ACLOCAL_FLAGS

echo "  autoheader"
autoheader

echo "  creating config_types.h.in"
cat >config_types.h.in <<EOF
/*
   generated header with missing types for the
   jbig2dec program and library. include this
   after config.h, within the HAVE_CONFIG_H
   ifdef
*/

#ifndef HAVE_STDINT_H
#  ifdef JBIG2_REPLACE_STDINT_H
#   include <@JBIG2_STDINT_H@>
#  else
    typedef unsigned @JBIG2_INT32_T@ uint32_t;
    typedef unsigned @JBIG2_INT16_T@ uint16_t;
    typedef unsigned @JBIG2_INT8_T@ uint8_t;
    typedef signed @JBIG2_INT32_T@ int32_t;
    typedef signed @JBIG2_INT16_T@ int16_t;
    typedef signed @JBIG2_INT8_T@ int8_t;
#  endif /* JBIG2_REPLACE_STDINT */
#endif /* HAVE_STDINT_H */
EOF
    
echo "  automake --add-missing $AUTOMAKE_FLAGS"
automake --add-missing $AUTOMAKE_FLAGS 
echo "  running autoconf"

autoconf

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
else
	echo "running ./configure $@"
fi

$srcdir/configure "$@" && echo
