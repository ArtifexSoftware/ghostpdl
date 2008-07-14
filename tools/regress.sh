#!/bin/sh

# regression testing program.

# get test file directory - we will run all files in that directory.
echo "Test files"
read TEST

# get directories for the old (working) and new (not working)
# executable.

echo "Old (working) executable directory"
read DIR1

echo "New (not working) executable directory"
read DIR2

# get the name of the executable for both old and new files
echo "executable name"
read EXE

# get the device - we assume the chosen device from the new exe device
# list is available on the old exe.

echo "choose available device(s) use a space to delimit devices"
(cd $DIR1; $EXE)
read DEVICES

# get the resolution
echo "resolution"
read RESOLUTION

# verify continuation - exit program if not ok.
echo $TEST
echo $DIR1
echo $DIR2
echo $EXE
echo $DEVICES
echo $RESOLUTION

echo "enter yes to continue"
read ANSWER

if [ "$ANSWER" != yes ]
then
    echo "bye"
    exit 0
fi
    
# for all devices
for device in $DEVICES
do
    echo processing $device
    # for all of the files in the test file directory 
    for file in $TEST
    do
	# print test file name
	echo processing $file
	# calculate checksum for new and old exe.  Note we always force banding.
	CHECKSUM1=`(cd $DIR1; $EXE -Z@ -r$RESOLUTION -K40000 -dBATCH -dNOPAUSE -dMaxBitmap=200000 -dBufferSpace=200000 -sOutputFile="|sum" -sDEVICE=$device $file | grep "^[0-9]" 2> /dev/null)`
	CHECKSUM2=`(cd $DIR2; $EXE -Z@ -r$RESOLUTION -K40000 -dBATCH -dNOPAUSE -dMaxBitmap=200000 -dBufferSpace=200000 -sOutputFile="|sum" -sDEVICE=$device $file | grep "^[0-9]" 2> /dev/null)`
	# if the check sums are different report them and the file name.
	if [ "$CHECKSUM1" != "$CHECKSUM2" ]
	then
	    echo checksum1 $CHECKSUM1
	    echo checksum2 $CHECKSUM2
	    echo bad checksum $file
	fi
    done
done