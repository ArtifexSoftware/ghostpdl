#!/bin/sh
# convert casper tree to local tree.
perl -pi.bak -e 's/.*/\/home\/henrys\/cvsroot2/' $(find . -name Root)
