#! /bin/bash

# intertactive or automatic

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
for x in common docs gs main pcl pxl tools urwfonts
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
    
    for x in common main pcl pxl tools gs
    do
        # build the news file name, if you don't have perl...
        # news-directory all upcase.
        UPCASE_DIR=$(echo $x | perl -pe 'tr/[a-z]/[A-Z]/')
        NEWS_FILE="NEWS-"$UPCASE_DIR

        # get the new logs
        cd $RELEASE_DIR/$x
        LOG=$(../$RELEASE_DIR/tools/cvs2log.py -L$NEWS_FILE -h artifex.com)
        # if the log is empty continue
        if test -z "$LOG"
        then
            echo "No code changes in the $x directory"
        else # LOG not null
            echo ""
            echo "$LOG"
            echo "Merge new logs for $x? (y/n)"
            read MERGE_LOGS
            # merge the new logs with the old logs
            if test $MERGE_LOGS = "y" || test $MERGE_LOGS = "Y"
            then
                # read old header the header is delimited by ^Version
                HEADER=""
                # preserve leading whitespace in the reads
                IFS="
"
                cat $NEWS_FILE | while read LINE
                do
                    if echo "$LINE" | grep "^Version" > /dev/null
                    then
                        # print the previous header, new header and
                        # log to a temporary file
                        printf "$HEADER" > "$NEWS_FILE.tmp"
                        echo "" >> "$NEWS_FILE.tmp"
                        echo "Version $VERSION ($(date '+%m/%d/%Y'))" >> "$NEWS_FILE.tmp"
                        echo "======================================" >> "$NEWS_FILE.tmp"
                        echo "$LOG" >> "$NEWS_FILE.tmp"
                        # print the old logs.
                        PRINT_LINE=""
                        cat $NEWS_FILE | while read LINE
                        do
                            if echo "$LINE" | grep "^Version" > /dev/null
                            then
                                PRINT_LINE="1"
                            fi
                            if test ! -z "$PRINT_LINE"
                            then
                                echo "$LINE" >> "$NEWS_FILE.tmp"
                            fi
                        done
                        break
                    else
                        HEADER="$HEADER\n$LINE"
                    fi
                done
                cp $NEWS_FILE.tmp $NEWS_FILE
            fi # merge logs - yes
        fi # LOG not null case.
        # back to release directory and continue.
        cd -
    done # NEWS file loop end
    # Update version in makefile
fi # update NEWS file condition
# update news files

echo ""
echo "Update pl.mak file? (y/n)"
read PLMAK_UPDATE
if test $PLMAK_UPDATE = "y" || test $PLMAK_UPDATE = "Y"
then
    perl -pi -e 's/^PJLVERSION=.*/PJLVERSION=1.35/' $RELEASE_DIR/pl/pl.mak
fi

echo ""
echo "Committing NEWS Files, tarring the release, ready (y/n)"
read COMMIT
if test $COMMIT = "y" || test $COMMIT = "Y"
then
	(cd $RELEASE_DIR; cvs commit;
	(cd ..; tar --exclude CVS -czvf ghostpcl_$VERSION.tar.gz $RELEASE_DIR))
fi
