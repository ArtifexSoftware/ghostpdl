#! /bin/bash

echo ""
echo "Enter directory of the release"
read RELEASE_DIR

#convert to absolute. no error checking
RELEASE_DIR=$(cd $RELEASE_DIR && pwd)

if test ! -d $RELEASE_DIR
then
    echo "can't find Release Directory $RELEASE_DIR"
    exit 1
fi

echo ""
echo "Enter version name: "
read VERSION

# no error checking other than "it's not null"

if test -z "$VERSION"
then
    echo "no version number provided"
    exit 1
fi

# check if this is a "plausible release "
for x in common doc gs main pcl pxl tools urwfonts
do
    if test ! -d $RELEASE_DIR/$x
    then
        echo "misssing directory $RELEASE_DIR/$x"
        exit 1
    fi
done

# create news files
echo ""
echo "create news file? (y/n)"
read NEWS_UPDATE

if test $NEWS_UPDATE = "y" || test $NEWS_UPDATE = "Y"
then
    
     NEWS_FILE="NEWS"
     # get the new logs
     echo "" > $RELEASE_DIR/$NEWS_FILE
     echo "GhostPCL log entries in reverse chronological order" >> $RELEASE_DIR/$NEWS_FILE
     echo "" >> $RELEASE_DIR/$NEWS_FILE
     echo "Current version is $VERSION ($(date '+%m/%d/%Y'))" >> $RELEASE_DIR/$NEWS_FILE
     echo "" >> $RELEASE_DIR/$NEWS_FILE
     (cd $RELEASE_DIR; $RELEASE_DIR/tools/cvs2log.py -h artifex.com) >> $RELEASE_DIR/$NEWS_FILE
fi # update NEWS file condition

echo ""
echo "Update pl.mak file? (y/n)"
read PLMAK_UPDATE
if test $PLMAK_UPDATE = "y" || test $PLMAK_UPDATE = "Y"
then
    perl -pi -e s/^PJLVERSION=.\*/PJLVERSION=$VERSION/ $RELEASE_DIR/pl/pl.mak
fi


echo ""
echo "Committing pl.mak (y/n)"
read COMMIT
if test $COMMIT = "y" || test $COMMIT = "Y"
then
	cvs commit $RELEASE_DIR/pl/pl.mak
fi

echo "making tar ball"
tar -C $RELEASE_DIR/.. --exclude CVS -czvf ghostpcl_$VERSION.tar.gz $(basename $RELEASE_DIR)

