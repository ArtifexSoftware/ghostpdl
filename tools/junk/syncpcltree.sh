
#!/bin/sh

rsync --delete -v -z -a -e ssh casper.ghostscript.com:/home/henrys/cvsroot2/ /home/henrys/cvsroot2/
