#!/bin/tcsh

set machine=REPLACE_WITH_YOUR_MACHINE_NAME

if (-e run.pid) then

find . -maxdepth 1 -name run.pid -mmin +120 -type f -print | xargs /bin/rm -f

else

ssh -i ~/.ssh/cluster_key regression@casper.ghostscript.com touch /home/regression/cluster/$machine.up
scp -i ~/.ssh/cluster_key  -q regression@casper.ghostscript.com:/home/regression/cluster/$machine.start . >&/dev/null
if (-e $machine.start) then
  ssh -i ~/.ssh/cluster_key regression@casper.ghostscript.com rm /home/regression/cluster/$machine.start
  scp -i ~/.ssh/cluster_key -q regression@casper.ghostscript.com:/home/regression/cluster/run.pl . >&/dev/null
  chmod +x run.pl
  perl run.pl $machine >&$machine.out
endif

endif

