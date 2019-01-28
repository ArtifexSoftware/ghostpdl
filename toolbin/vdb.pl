#!/usr/bin/perl

# Perl script to ease the use of gdb with valgrind.
#
# Invoke as: vdb.pl <command to run>

use strict;
use warnings;
use IPC::Open3;
use IO::Select;

# Global variables
my $gdbkilled = 0;

sub killgdb() {
    if ($gdbkilled) {
        return;
    }
    print GDBSTDIN " \nkill\ny\nquit\n";
    $gdbkilled = 1;
}


# Store the args
my @args = @ARGV;

# Make the invocation args for valgrind
my @vgargs = (
    "valgrind",
    "--track-origins=yes",
    "--vgdb=yes",
    "--vgdb-error=0" );
push(@vgargs, @args);

# Make the invocation args for gdb
my @gdbargs = (
    "gdb",
    $args[0]);

# Fork the subprocesses
my $vgpid = open3(0, \*VGSTDOUT, 0, @vgargs);
my $gdbpid = open3(\*GDBSTDIN, \*GDBSTDOUT, 0, @gdbargs);

# Cope with Ctrl-C
sub my_sigint_handler() {
    print "Caught a SIGINT...\n";
    if ($gdbkilled) {
        kill 9, $vgpid;
        kill 9, $gdbpid;
        exit(1);
    }
    killgdb();
}

$SIG{'INT'} = \&my_sigint_handler;

my $sel = new IO::Select();
$sel->add(*VGSTDOUT);
$sel->add(*GDBSTDOUT);
$sel->add(*STDIN);

*STDOUT->autoflush();

my $scanning = 1;

sub print_lines($)
{
    my $buf=shift;

    while (1) {
	my $loc = index($buf, "\n");
	if ($loc < 0) {
	    last;
	}
	my $line = substr($buf, 0, $loc+1);
	print "$line";
	$buf = substr($buf, $loc+1);
    }
    return $buf;
}

my $vgpartial = '';
my $gdbpartial = '';
my $last2print = 0; # 0 = VG, 1 = GDB
while (my @ready = $sel->can_read())
{
    for my $fh (@ready) {
        # If valgrind says anything, just print it.
        if ($fh eq *VGSTDOUT) {
            my $vgbuf= '';
            if (sysread(VGSTDOUT, $vgbuf, 64*1024, length($vgbuf)) == 0) {
                # When valgrind hits EOF, exit.
                killgdb();
                waitpid($vgpid,0);
                waitpid($gdbpid,0);
                exit(0);
            }
            if ($scanning) {
                $vgbuf =~ m/(target remote \| .+ \-\-pid\=\d+)\s*/;
                if ($1) {
                    print GDBSTDIN "$1\n";
                    $scanning = 0;
                }
            }
	    # It definitely read something, so print it.
	    if ($last2print == 1) { # Last to print was GDB
                if ($gdbpartial ne "") { # We need a newline
		    print "\n";
		}
		# Better reprint any partial line we had
		print "$vgpartial";
	    }
            $vgpartial = print_lines($vgbuf);
	    print "$vgpartial";
	    $last2print = 0; # VG
        }
        # Don't say anything to or from gdb until after we've got the magic words from valgrind
        if ($scanning == 0) {
            # Anything the user says, should be parotted to gdb
            if ($fh eq *STDIN) {
                my $buf = '';
                if (sysread(STDIN, $buf, 64*1024, length($buf)) == 0) {
                    # When the user hits EOF, start to kill stuff.
                    killgdb();
                }
                print GDBSTDIN "$buf";
            }
            # Anything gdb says, should be parotted out.
            if ($fh eq *GDBSTDOUT) {
		my $gdbbuf='';
                if (sysread(GDBSTDOUT, $gdbbuf, 64*1024, length($gdbbuf)) == 0) {
                    # When gdb hits EOF start to kill stuff.
                    killgdb();
                }
		# It definite read something, so print it.
		if ($last2print == 0) { # Last to print was VG
		    if ($vgpartial ne "") { # We need a newline
			print "\n";
		    }
		    # Better reprint any partial line we had
		    print "$gdbpartial";
		}
		$gdbpartial = print_lines($gdbbuf);
		print "$gdbpartial";
		$last2print = 1; # GDB
            }
        }
    }
}
