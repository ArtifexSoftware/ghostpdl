#!/bin/sh

# this is a script makes a snapshot of the ghostpdl/gs trunk and 
# uploads it casper. 
# Note that all the code gets pulled to wherever you run it and then
# pushed back to casper, so it takes a few minutes.

# It constructs the snapshot from a clean export from the repository,
# but BASED ON THE REVISIONS IN THE CURRENT CHECKOUT it's run from.
# A little confusing, but it gives you some control over the version
# posted. Run 'svn up' first if you want to do the latest.

# this script must be run from the top level of a ghostpdl working copy

# cut off revision numbers for the changelogs
NEWSREV=8840

# svn urls
SVNROOT=http://svn.ghostscript.com/ghostscript/trunk
ghostpdl_SVNROOT=${SVNROOT}/ghostpdl
gs_SVNROOT=${SVNROOT}/gs

REV=`svn info | grep Revision: | cut -f 2 -d ' '`

CHANGELOG="ghostpdl-r${REV}_NEWS.txt"
echo "creating changelog... ${CHANGELOG}"
svn log -r${REV}:${NEWSREV} $ghostpdl_SVNROOT > ${CHANGELOG}

exportdir="ghostpdl-r${REV}"
if test -d "$exportdir"; then
  echo "export target directory $exportdir already exists!"
  exit 1
fi

echo "creating ${exportdir}.tar.gz ..."

# export the source code
svn export -q -r ${REV} $ghostpdl_SVNROOT ${exportdir}

# remove proprietary subdirectories that might exist
echo "removing proprietary code ... "
for verboten in ufst tools/metro_tests; do
  if test -d $exportdir/$verboten; then
    echo "  $verboten"
    rm -rf $exportdir/$verboten
  fi
done

# copy in the changelogs
cp ${CHANGELOG} ${exportdir}
tar czf $exportdir.tar.gz $exportdir/*
rm -rf $exportdir

#echo $exportdir.tar.gz ready for upload
echo "uploading changelogs and $exportdir.tar.gz ..."
scp ${CHANGELOG} $exportdir.tar.gz \
  www.ghostscript.com:/www/ghostscript.com/snapshots/

echo "updating links ..."
echo "cd /www/ghostscript.com/snapshots/ && \
  if test -w ghostpdl-current.tar.gz; then \
   rm ghostpdl-current.tar.gz; \
   ln -s ${exportdir}.tar.gz ghostpdl-current.tar.gz; \
  fi; \
  if test -w ghostpdl-current_NEWS.txt; then \
   rm ghostpdl-current_NEWS.txt; \
   ln -s ${CHANGELOG} ghostpdl-current_NEWS.txt; \
  fi;" \
  # maintain the legacy current tarball link \
  if test -w ghostpcl-current.tar.gz; then \
   rm ghostpcl-current.tar.gz; \
   ln -s ${exportdir}.tar.gz ghostpcl-current.tar.gz; \
  fi \
	| ssh ghostscript.com
