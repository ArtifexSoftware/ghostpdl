#! /bin/bash

echo ""
echo "Enter directory of the release"
read RELEASE_DIR

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

# update news files
echo ""
echo "Update news files? (y/n)"
read NEWS_UPDATE

if test $NEWS_UPDATE = "y" || test $NEWS_UPDATE = "Y"
then
    
    (cd $RELEASE_DIR; tools/cvs2log.py -h artifex.com > ChangeLog)
fi

echo ""
echo "Update pl.mak file? (y/n)"
read PLMAK_UPDATE
if test $PLMAK_UPDATE = "y" || test $PLMAK_UPDATE = "Y"
then
    perl -pi -e "s/^PJLVERSION=.*/PJLVERSION=$VERSION/" $RELEASE_DIR/pl/pl.mak
fi

echo ""
echo "Commit changes (y/n)"
read COMMIT
if test $COMMIT = "y" || test $COMMIT = "Y"
then
	(cd $RELEASE_DIR; cvs commit)
fi

echo ""
echo "Tar ready (y/n)"
read TAR
if test $TAR = "y" || test $TAR = "Y"
then
    BASEDIR_NAME=$(basename "$RELEASE_DIR")
    RELEASE_NAME=$BASEDIR_NAME-$VERSION
    (cd $RELEASE_DIR; cd ..;
    mv $BASEDIR_NAME $RELEASE_NAME
    tar --exclude CVS -czvf $RELEASE_NAME.tar.gz $RELEASE_NAME
    mv $RELEASE_NAME $BASEDIR_NAME)
fi
