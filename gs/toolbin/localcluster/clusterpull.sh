#!/bin/tcsh

set machine=REPLACE_WITH_YOUR_MACHINE_NAME


ssh -i ~/.ssh/cluster_key regression@casper.ghostscript.com touch /home/regression/cluster/$machine.up

if (-e $machine.start) then
else

touch $machine.start
rm -f $machine.start

scp -i ~/.ssh/cluster_key  -q regression@casper.ghostscript.com:/home/regression/cluster/$machine.start . >&/dev/null

if (-e $machine.start) then
  ssh -i ~/.ssh/cluster_key regression@casper.ghostscript.com rm /home/regression/cluster/$machine.start

  scp -i ~/.ssh/cluster_key -q regression@casper.ghostscript.com:/home/regression/cluster/$machine.jobs.gz . >&/dev/null
  ssh -i ~/.ssh/cluster_key regression@casper.ghostscript.com rm /home/regression/cluster/$machine.jobs.gz

  scp -i ~/.ssh/cluster_key -q regression@casper.ghostscript.com:/home/regression/cluster/run.pl . >&/dev/null
  chmod +x run.pl

  touch $machine.jobs
  rm $machine.jobs
  gunzip $machine.jobs.gz

  perl run.pl $machine >&$machine.out


endif
endif

