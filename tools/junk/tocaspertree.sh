#!/bin/sh

# change repository to casper
perl -pi.bak -e 's/.*/casper.ghostscript.com:\/home\/henrys\/cvsroot2/' $(find . -name Root)
