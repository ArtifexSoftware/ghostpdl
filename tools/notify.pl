#!/usr/bin/perl -w

# args are email addresses - no args stdout
@EMAIL_LIST=@ARGV;
# NB hardwired to look at logs for the past day.
open(INPUT, "tools/cvs2log.py '-d>=1 day ago' -h artifex.com|");

$LOG_ENTRIES="";
while (<INPUT>) {
    $LOG_ENTRIES .= $_;
}

if ( $LOG_ENTRIES ne "" ) {
    # if we have an email list go to stdout else cat to the terminal.
    if (@EMAIL_LIST) {
	open(OUTPUT, "| mail -s 'Change Logs for last 24 hours' @EMAIL_LIST");
    } else {
	open(OUTPUT, "| cat");
    }
    print OUTPUT $LOG_ENTRIES;
    print OUTPUT "\n";
    close OUTPUT;
}
close INPUT;

