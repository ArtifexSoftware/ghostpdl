#!/usr/bin/env python

# Return the cvs repository - see CVS/Repository.
def GetCVSRepository():
    try:
	fp = open('CVS/Root', 'r')
    except:
        try:
            # should search but we cheat.
            fp = open('pcl/CVS/Root', 'r')
        except:
            return None
    # get the Root name and strip off the newline
    repos = fp.readline()[:-1]
    fp.close()
    return repos

def GetCurrentWorkingDirectory():
    import os
    return os.getcwd()

# figure out what day it is given year month and day

def weekday(year, month, day):
    import time
    seconds = time.mktime(year, month, day, 0, 0, 0, 0, 0, 0)
    tm = time.localtime(seconds)
    return tm[6]


# convert an rcs time to a tuple that C/python time functions can work
# with.  No error checking here.  Precision to hours only.
def RcsDate2CtimeTuple(date_data):
    import string
    (date, time) = string.splitfields(date_data, ' ')
    (year, month, day) = string.splitfields(date, '/')
    (hours, mins, secs) = string.splitfields(time, ':')
    year = string.atoi(year)
    month = string.atoi(month)
    day = string.atoi(day)
    hours = string.atoi(hours)
    return (year, month, day, hours, 0, 0, weekday(year, month, day), 0, 0)

# get the last date from a log file return the time tuple of the last
# log entry or return or return Jan 1, 1970.  This date is the start
# point for logging
def LastLogDate2CtimeTuple(filename):
    import string
    day_abbr = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
    month_abbr = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']
    date_tuple = (1970, 1, 1, 0, 0, 0, 0, 0, 0)
    print filename
    try:
	fp = open(filename, 'r')
    except:
	print "couldn't open log file: " + filename
	return date_tuple
    while 1:
	line = fp.readline()
	if not line:
	    break
	# we could test more but ...
	if line[0:3] in day_abbr and line[4:7] in month_abbr and (0 <= string.atoi(string.strip(line[8:10])) <= 31):
	    (dd, mm, dn, t, yy) = string.split(line[:24])
	    (th, tm, ts) =  string.splitfields(t, ':')
	    dd=day_abbr.index(dd)
	    yy=string.atoi(yy)
            mm=month_abbr.index(mm) + 1
	    dn=string.atoi(dn)
	    th=string.atoi(th)
	    tm=string.atoi(tm)
	    date_tuple = (yy, mm, dn, th, tm, 0, dd, 0, 0)
	    break
    return date_tuple

# build a date, time, author, email address header.
def ChangeLogDateNameHeader(date, author, hostname, tabsize):
    import time, pwd
    seperator = ' ' * tabsize
    try:
	author_name = pwd.getpwnam(author)[4]
    except:
	author_name = ''
    return time.asctime(date) + ' GMT' + seperator + author_name +\
	   seperator + author + '@' + hostname + '\n'

# build change log entries with file name, revisions, etc. lumping
# like descriptions together.  We assume the dictionary contains data.
def ChangeLogFileRevDesc(entry_dict, indent, line_length):
    leading_space = ' ' * indent

    change_log_entry = ''
    pos = 0
    for description in entry_dict.keys():
	change_log_entry = change_log_entry + leading_space + '* '
	pos = len(leading_space + '* ')
	# get the revisions and rcs files associated with the
	# description.
	for revision, rcs_file in entry_dict[description]:
	    # NB do a better wrapping job.
	    if pos > line_length:
		change_log_entry = change_log_entry + '\n' + leading_space
		pos = 0
	    change_log_entry = change_log_entry + rcs_file + ' [' + revision + ']' + ', '
	    pos = pos + len(rcs_file + ' [' + revision + ']' + ', ')
	# replace last ', ' with a semicolon - strings are immutable so
	# an inplace assignment won't do.
	change_log_entry = change_log_entry[:-2] + ':\n\n'
	# add on the current description
	change_log_entry = change_log_entry + description + '\n'
    return change_log_entry


# Build the cvs log.
def BuildLog(log_date_command):
    import os, string
    reading_description = 0
    description = []
    log = []
    # start reading cvs log output putting what we need in the log
    # list
    for line in os.popen(log_date_command, 'r').readlines():
	if line[:5] == '=====' or line[:5] == '-----':
	    if description != []:
		# append these items in the sort order we'll want later on.
		log.append(RcsDate2CtimeTuple(date), author, description,
			   rcs_file[len(GetCVSRepository()):-3], revision[:-1])
	    reading_description = 0
	    description = []
	elif not reading_description and line[:len("RCS file: ")] == "RCS file: ":
	    rcs_file = line[len("RCS file: "):]
	elif not reading_description and line[:len("revision ")] == "revision ":
	    revision = line[len("revision "):]
	elif not reading_description and line[:len("date: ")] == "date: ":
	    (dd, aa, ss, ll) = string.splitfields(line, ';')
	    (foo, date) = string.splitfields(dd, ': ')
	    (foo, author) = string.splitfields(aa, ': ')
	    reading_description = 1
	elif reading_description:
	    description.append(line)
	else:
	    continue
    return log

def main():
    import sys, getopt, time, string, socket
    try:
	opts, args = getopt.getopt(sys.argv[1:], "d:i:h:l:u:r:Rt:DL:",
				   ["date",
                                    "indent",
				    "hostname",
				    "length",
				    "user",
				    "rlog_options", #### not yet supported
				    "Recursive",    #### not yet supported
				    "tabwidth",
				    "Diffs",        #### not yet supported
				    "Logfile"])
    except getopt.error, msg:
	sys.stdout = sys.stderr
	print msg
	print "Usage: Options:"
	print "[-h hostname] [-i indent] [-l length] [-R]"
	print "[-r rlog_option] [-d rcs date]\n[-t tabwidth] [-u 'login<TAB>fullname<TAB>mailaddr']..."
	sys.exit(2)

    # Set up defaults for all of the command line options.
    cvsrepository=GetCVSRepository()
    if not cvsrepository:
	print "cvs2log.py must be executed in a working CVS directory"
	sys.exit(2)
    indent=8
    hostname=socket.gethostbyaddr(socket.gethostname())[1][0]
    length=79
    user=0
    recursive=0
    tabwidth=8
    rlog_options=0
    diffs=1
    logfile=None
    date_option = ""
    # override defaults if specified on the command line
    for o, a in opts:
	if o == '-h' : hostname = a
	elif o == '-i' : indent = a
	elif o == '-l' : length = a
	elif o == '-t' : tabwidth = a
	elif o == '-u' : user = a
	elif o == '-r' : rlog_options = a
	elif o == '-R' : recursive = 1
        elif o == '-d' : date_option = "-d " + "'" + a + "'"
	elif o == '-D' : diffs = 0
	elif o == '-L' : logfile = a
	else: print "getopt should have failed already"

    # set up the cvs log command arguments.  If a logfile is specified
    # we get all dates later than the logfile.
    if logfile:
	date_option = "'" + "-d>" + time.asctime(LastLogDate2CtimeTuple(logfile)) + "'"

    log_date_command= 'cvs -d ' + GetCVSRepository() + ' -Q log -N ' + date_option
    # build the logs.
    log = BuildLog(log_date_command)
    # chronologically reversed.  We reverse everything in the list as
    # well.
    log.sort()
    log.reverse()
    # Pass through the logs creating new entries based on changing
    # authors, dates and descriptions.
    last_date = None
    last_author = None
    entry_dict = {}
    for date, author, description, rcs_file, revision in log:
	if author != last_author or date != last_date:
	    # clear out any old revisions, descriptions, and filenames
	    # that belong with the last log.
	    if entry_dict:
		print ChangeLogFileRevDesc(entry_dict, indent, length)
	    # clear the entry log for the next group of changes
	    # if we have a new author or date we print a date, time,
	    # author, and email address.
	    entry_dict = {}
	    print ChangeLogDateNameHeader(date, author, hostname, tabwidth)

	# build string from description list so we can use it as a key
	# for the entry dictionary
	description_data = indent * ' '
	for line in description:
	    description_data = description_data + line + indent * ' '
	# put the revisions and rcs files in decription-keyed
	# dictionary.
	if entry_dict.has_key(description_data):
	    entry_dict[description_data].append(revision, rcs_file)
	else:
	    entry_dict[description_data] = [(revision, rcs_file)]

	last_author = author
	last_date = date
	last_description = description

    # print the last entry if there is one (i.e. the last two entries
    # have the same author and date)
    if entry_dict:
	print ChangeLogFileRevDesc(entry_dict, indent, length)

if __name__ == '__main__':
    main()
