#!/usr/bin/env python

# script to notify users of the tree changes.
# 

import string, os

cvs_command = "cvs status"

working_rev = ''
repos_rev = ''
component = ''
file_name = ''

# for read each record seperated by ==
for line in os.popen(cvs_command, 'r').readlines():
    if line[:5] == '=====':
	if working_rev and repos_rev and component and file_name:
	    if (working_rev != repos_rev):
		command = "cvs rdiff -r" + working_rev + " -r" + repos_rev + ' ' + component + '/' + file_name
		for diff_line in os.popen(command, 'r').readlines():
		    print diff_line,
	    working_rev = ''
	    repos_rev = ''
	    component = ''
	    file_name = ''
    elif line[3:20] == 'Working revision:':
	working_rev = line[21:-1]
    elif line[3:23] == 'Repository revision:':
	repos_rev = string.split(line[24:])[0]
	# wee....
	file_name = string.split(string.split(line, '/')[-1], ',')[0]
	component= string.split(line, '/')[-2]
