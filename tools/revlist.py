#!/usr/bin/env python

# script to parse svn log outputs from ghostpcl and gs
# and merge them into a single list for time-based reference
# run from the top level of the ghostpcl source tree

# may be brittle as it's based on parsing human-readable output.

import os, string

SVN_CMD = 'svn log -q'

# array to hold the revision data
entries = []

def parse(log, prefix=None):
  '''parse the output of SVN_CMD and dump the results
     as tuples   in the entries global'''
  if prefix: prefix += '-'
  else: prefix = ''
  for line in log.readlines():
    if line[0] != 'r': continue
    fields = line.split('|')
    fields = map(string.strip, fields)
    rev = prefix + fields[0]
    author = fields[1]
    date = fields[2]
    entries.append((date, rev, author))

log = os.popen(SVN_CMD)
parse(log, 'ghostpcl')
log.close()

log = os.popen(SVN_CMD + ' gs')
parse(log, 'ghostscript')
log.close()

# sort (by date, since that element is first)
entries.sort()

# print out the results
tab = reduce(max, [len(entry[1]) for entry in entries])
for entry in entries:
  date = entry[0]
  rev = entry[1]
  author = entry[2]
  pad = tab - len(rev)
  print rev, ' ' * pad, date, author
