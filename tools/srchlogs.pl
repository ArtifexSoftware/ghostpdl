#!/usr/bin/perl -w

# args are email addresses - no args stdout
@EMAIL_LIST=@ARGV;
# NB hardwired to look at logs for the past day.
open(INPUT, "cvs -Q -d:pserver:henrys\@meerkat.dimensional.com:/home/henrys/cvsroot log -N '-d>=1 day ago'|");

# only true if there are logs.
$open_output = 0;

while (<INPUT>) {
    # grab the working file in case this is a real log entry.
    if (/^Working file:\s*(.*)/) {
	$working_file = $1;
    }
    # real log entry dashed lines starts summary stats.
    if (/^----------------------------/) {
	# open output only once if we have a real log entry, mail
	# will send a null message if it is opened and not sent data.
	if ($open_output == 0) {
	    # if we have an email list go to stdout else cat to the terminal.
	    if (@EMAIL_LIST) {
		open(OUTPUT, "| mail -s 'Change Logs for last 24 hours' @EMAIL_LIST");
	    } else {
		open(OUTPUT, "| cat");
	    }
	    $open_output = 1;
	}
	# create new summary paragraph
	$summary = "";
	# append summary lines until we reach double dash line
      DESCLINE:
	while (<INPUT>) {
	    last DESCLINE if /^===========================/;
	    $summary .= $_;
	}
	print OUTPUT "$working_file ", "$summary\n";

    }
}

# cleanup open files
if ($open_output == 1) {
    close OUTPUT;
}
close INPUT;
