#!/bin/sh

# this is a script to make a snapshot of ghostpcl+gs trunk and upload
# it casper. This is mostly because casper has an old svn installation
# which can't be conveniently upgraded, so we have to do this manually.
# Note that all the code gets pulled to wherever you run it and then
# pushed back to casper, so it takes a few minutes.

# it constructs a snapshot from a clean export from the repository,
# but BASED ON THE REVISIONS IN THE CURRENT CHECKOUT it's run from.
# a little confusing, but it gives you some control over the version
# posted. Run 'svn up' first if you want to do the latest.

# this script must be run from the top level of a ghostpcl working copy
# requires svn export --ignore-externals, which is svn 1.2 or later I think

# cut off revision numbers for the changelogs
ghostpcl_NEWSREV=3034
ghostscript_NEWSREV=8567

# svn urls
ghostpcl_SVNROOT=svn+ssh://svn.ghostscript.com/var/lib/svn-private/ghostpcl/trunk/ghostpcl
ghostscript_SVNROOT=svn+ssh://svn.ghostscript.com/svn/ghostscript/trunk/gs

ghostpcl_REV=`svn info | grep Revision: | cut -f 2 -d ' '`
ghostscript_REV=`svn info gs | grep Revision: | cut -f 2 -d ' '`

echo "creating changelogs ..."
echo "  ghostscript-r${ghostscript_REV}_NEWS.txt"
svn log -r${ghostscript_REV}:${ghostscript_NEWSREV} $ghostscript_SVNROOT \
	> ghostscript-r${ghostscript_REV}_NEWS.txt
echo "  ghostpcl-r${ghostpcl_REV}_NEWS.txt"
svn log -r${ghostpcl_REV}:${ghostpcl_NEWSREV} $ghostpcl_SVNROOT \
	> ghostpcl-r${ghostpcl_REV}_NEWS.txt

exportdir="ghostpcl-r${ghostpcl_REV}+${ghostscript_REV}"
if test -d "$exportdir"; then
  echo "export target directory $exportdir already exists!"
  exit 1
fi

echo "creating ${exportdir}.tar.gz ..."

# export the ghostpcl code
svn export -q --ignore-externals -r ${ghostpcl_REV} $ghostpcl_SVNROOT \
	$exportdir
# export ghostscript code
cd $exportdir
svn export -q -r ${ghostscript_REV} $ghostscript_SVNROOT gs
cd ..

# remove proprietary subdirectories
echo "removing proprietary code ... "
for verboten in ufst tools/metro_tests; do
  if test -d $exportdir/$verboten; then
    echo "  $verboten"
    rm -rf $exportdir/$verboten
  fi
done

# copy in the changelogs
cp ghostpcl-r${ghostpcl_REV}_NEWS.txt ghostscript-r${ghostscript_REV}_NEWS.txt \
	$exportdir
tar czf $exportdir.tar.gz $exportdir/*
rm -rf $exportdir

#echo $exportdir.tar.gz ready for upload
echo "uploading changelogs and $exportdir.tar.gz ..."
scp ghostpcl-r${ghostpcl_REV}_NEWS.txt \
	ghostscript-r${ghostscript_REV}_NEWS.txt \
	$exportdir.tar.gz \
  www.ghostscript.com:/www/ghostscript.com/snapshots/

echo "updating links ..."
echo "cd /www/ghostscript.com/snapshots/ && \
  if test -w ghostpcl-current.tar.gz; then \
   rm ghostpcl-current.tar.gz; \
   ln -s ${exportdir}.tar.gz ghostpcl-current.tar.gz; \
  fi; \
  if test -w ghostpcl-current_NEWS.txt; then \
   rm ghostpcl-current_NEWS.txt; \
   ln -s ghostpcl-r${ghostpcl_REV}_NEWS.txt ghostpcl-current_NEWS.txt; \
  fi" \
	| ssh ghostscript.com
