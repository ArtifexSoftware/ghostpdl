#!/usr/bin/env python

# script to parse svn log outputs from ghostpcl and gs
# and merge them into a single list for time-based reference
# run from the top level of the ghostpcl source tree

# may be brittle as it's based on parsing human-readable output.

import os, string
import sys

SVN_CMD = 'svn log -q'
SVN_GS_REPOS = 'svn+ssh://svn.ghostscript.com/svn/ghostscript/trunk/gs'
SVN_PCL_REPOS = 'svn+ssh://svn.ghostscript.com/var/lib/svn-private/ghostpcl'

def parse(log, entries, module=None):
  '''parse the output of SVN_CMD and dump the results
     as tuples   in the entries global'''
  if not module: module = ''
  for line in log.readlines():
    if line[0] != 'r': continue
    fields = line.split('|')
    fields = map(string.strip, fields)
    rev = fields[0][1:] # strip the initial 'r'
    author = fields[1]
    date = fields[2]
    entries.append((date, rev, author, module))

def report(entries):
  'print out the results'

  # Every ghostscript revision creates a ghostpcl revision as
  # well, during the period we've had an svn:external for it
  last_gs_rev = 0
  last_pcl_rev = 0

  tab = reduce(max, [len(entry[1]) for entry in entries])

  for entry in entries:
    date = entry[0]
    rev = entry[1]
    author = entry[2]
    module = entry[3]
    if module == 'ghostscript':
      last_gs_rev = int(rev)
      if last_pcl_rev >= 2609:
        rev = str(last_pcl_rev) + '+' + rev
      else:
        continue
    elif module == 'ghostpcl':
      last_pcl_rev = int(rev)
      if last_pcl_rev >= 2609:
        rev = rev + '+' + str(last_gs_rev)
    pad = tab - len(rev)
    print rev, ' ' * pad, date, author
    
if __name__ == '__main__':
  sys.stderr.write('Downloading logs. This may take a while...\n')

  # array to hold the revision data
  entries = []

  log = os.popen(SVN_CMD + ' ' + SVN_PCL_REPOS)
  parse(log, entries, 'ghostpcl')
  log.close()

  log = os.popen(SVN_CMD + ' ' + SVN_GS_REPOS)
  parse(log, entries, 'ghostscript')
  log.close()

  # sort (by date, since that element is first)
  entries.sort()

  report(entries)
