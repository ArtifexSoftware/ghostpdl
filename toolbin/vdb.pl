#!/usr/bin/perl

# Perl script to ease the use of gdb with valgrind.
#
# Invoke as: vdb.pl <command to run>
#
# This also adds some commands to the usual gdb/valgrind
# stuff.
#
# xb:
#
#     xb expression
#   or:
#     xb expression_addr expression_length
#
#   This does what you'd hope:
#
#     mon xb <expression_addr> <expression_length>
#
#   but it copes with expressions for the address and length
#   rather than insisting on raw numbers.
#
# xs:
#
#   xs expression
#
#   This does what you'd hope:
#
#      mon xb <expression> <strlen(expression)>
#
#   would do.
#
# xv:
#
#  xv expression
#
#  This does what you'd hope:
#
#     mon xb &<expression> sizeof(<expression>)
#
#  would do.

use strict;
use warnings;
use IPC::Open3;
use IO::Select;

# Store the args
my @args = @ARGV;

# Wrap ourselves for line editing
if (!exists $ENV{'VDB_WRAPPED'}) {
    `which rlwrap`;
    if ($? != 0) {
	print "rlwrap not available - no command line editing.\n";
	print "Consider: sudo apt-get install rlwrap\n";
    } else {
	$ENV{'VDB_WRAPPED'}=1;
	unshift(@args, "rlwrap", $0);
	exec(@args);
    }
}

# Global variables
my $gdbkilled = 0;

sub killgdb() {
    if ($gdbkilled) {
        return;
    }
    print GDBSTDIN " \nkill\ny\nquit\n";
    $gdbkilled = 1;
}


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

my $rate_limit = 0;
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
	$rate_limit++;
	if ($rate_limit >= 24) {
            sleep(1);
	    $rate_limit = 0;
	}
        $buf = substr($buf, $loc+1);
    }
    return $buf;
}

# We scan the output of VG to look for the magic runes
#  State 0 = still waiting for magic runes
#        1 = runes have been captured.
my $vg_scan_state = 0;

# We scan the output from GDB to look for responses to commands we have given it.
#  State 0 = Normal output - just parrot it out.
#        1 = Waiting for the result of a print address
#        2 = Waiting for the result of a print length
my $gdb_scan_state = 0;

my $vgpartial = '';
my $gdbpartial = '';
my $inpartial = '';
my $last2print = 0; # 0 = VG, 1 = GDB
my $xb_addr;
my $xb_len;
sub go_command($) {
    my $command = shift;
    chomp $command;
                            if (!defined $command) {
                                # When valgrind hits EOF, exit.
                                killgdb();
                                waitpid($vgpid,0);
                                waitpid($gdbpid,0);
                                exit(0);
                            } elsif ($command =~ m/^\s*xb\s+(\S+)\s*$/) {
				my $one = $1;
                                if ($one =~ m/0x[a-fA-F0-9]*/) {
                                    $xb_addr="$one";
                                    $xb_len="8";
                                    print GDBSTDIN "mon xb $xb_addr $xb_len\n";
                                } else {
                                    # We need to figure out the value of xb_addr;
				    if ($one =~ m/^\&(.+)/) {
                                        $xb_len="sizeof($1)";
				    } else {
                                        $xb_len="sizeof(*$one)";
				    }
                                    print GDBSTDIN "print $one\n";
                                    $gdb_scan_state=1; # Next thing we get back will be the address
                                }
                            } elsif ($command =~ m/^\s*xv\s+(\S+)\s*$/) {
				my $one = $1;
                                # We need to figure out the value of xb_addr;
                                $xb_len="sizeof($one)";
                                print GDBSTDIN "print &$one\n";
                                $gdb_scan_state=1; # Next thing we get back will be the address
                            } elsif ($command =~ m/^\s*xb\s+(\S+)\s+(\S+)\s*$/) {
				my $one = $1;
				my $two = $2;
                                if ($one =~ m/0x[a-fA-F0-9]*/) {
                                    $xb_addr="$one";
				    if ($two =~ m/^\d+$/) {
                                        $xb_len="$two";
                                        print GDBSTDIN "mon xb $xb_addr $xb_len\n";
				    } else {
					print GDBSTDIN "print $two\n";
					$gdb_scan_state=2; # Next thing we get back will be the length
				    }
                                } else {
                                    # We need to figure out the value of xb_addr;
                                    $xb_len="$two";
                                    print GDBSTDIN "print $one\n";
                                    $gdb_scan_state=1; # Next thing we get back will be the address
				}
                            } elsif ($command =~ m/^\s*xs\s+(\S+)\s*$/) {
				my $one = $1;
                                if ($one =~ m/0x[a-fA-F0-9]*/) {
                                    $xb_addr="$one";
				    print GDBSTDIN "print strlen($one)\n";
				    $gdb_scan_state=2; # Next thing we get back will be the length
                                } else {
                                    # We need to figure out the value of xb_addr;
                                    $xb_len="strlen($one)";
                                    print GDBSTDIN "print $one\n";
                                    $gdb_scan_state=1; # Next thing we get back will be the address
                                }
                            } else {
                                print GDBSTDIN "$command\n";
                            }
}

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
            if ($vg_scan_state == 0) {
                $vgbuf =~ m/(target remote \| .+ \-\-pid\=\d+)\s*/;
                if ($1) {
		    print GDBSTDIN "$1\n";
                    $vg_scan_state = 1;
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
        if ($vg_scan_state != 0) {
	    if ($fh eq *STDIN) {
                my $inbuf=$inpartial;
                if (sysread(STDIN, $inbuf, 64*1024, length($inbuf)) == 0) {
                    # When gdb hits EOF start to kill stuff.
                    killgdb();
                }
                while (1) {
                    my $loc = index($inbuf, "\n");
                    if ($loc < 0) {
                        last;
                    }
                    my $line = substr($inbuf, 0, $loc+1);
		    go_command($line);
                    $inbuf = substr($inbuf, $loc+1);
                }
		$inpartial = $inbuf;
	    }
            # Anything gdb says, should be parotted out.
            if ($fh eq *GDBSTDOUT) {
                my $gdbbuf='';
                if (sysread(GDBSTDOUT, $gdbbuf, 64*1024, length($gdbbuf)) == 0) {
                    # When gdb hits EOF start to kill stuff.
                    killgdb();
                }
		while ($gdbbuf ne "") {
                    if ($gdb_scan_state == 0) {
                        # It definitely read something, so print it.
                        if ($last2print == 0) { # Last to print was VG
                            if ($vgpartial ne "") { # We need a newline
                                print "\n";
                            }
                            # Better reprint any partial line we had
                            print "$gdbpartial";
                        }
                        $gdbpartial = print_lines($gdbbuf);
			$gdbbuf="";
                        print "$gdbpartial";
                        $last2print = 1; # GDB
                    } elsif ($gdb_scan_state == 1) {
                        $gdbpartial .= $gdbbuf;
			$gdbbuf = "";
                        while (1) {
                            my $loc = index($gdbpartial, "\n");
                            if ($loc < 0) {
                                last;
                            }
                            my $line = substr($gdbpartial, 0, $loc+1);
                            $gdbpartial = substr($gdbpartial, $loc+1);
                            if ($line =~ m/\$\d+ =.*(0x[a-zA-Z0-9]+)/) {
                                $xb_addr=$1;
				print GDBSTDIN "print $xb_len\n";
                                $gdb_scan_state = 2; # Now we look for the length
				last;
                            }
			    if ($line =~ m/(.*\n)/) {
				$gdbpartial =~ s/(.*\n)//;
				print "$1";
				$gdbbuf =  $gdbpartial;
				$gdbpartial = "";
				$gdb_scan_state = 0; # Error
				last;
			    }
                        }
                    } elsif ($gdb_scan_state == 2) {
                        $gdbpartial .= $gdbbuf;
			$gdbbuf = "";
                        while (1) {
                            my $loc = index($gdbpartial, "\n");
                            if ($loc < 0) {
                                last;
                            }
                            my $line = substr($gdbpartial, 0, $loc+1);
                            $gdbpartial = substr($gdbpartial, $loc+1);
                            if ($line =~ m/\$\d+ = (\d+)/) {
                                $xb_len=$1;
                                $gdb_scan_state = 0;
				print GDBSTDIN "mon xb $xb_addr $xb_len\n";
				last;
                            }
			    if ($line =~ m/(.*\n)/) {
				$gdbpartial =~ s/(.*\n)//;
				print "$1";
				$gdbbuf =  $gdbpartial;
				$gdbpartial = "";
				$gdb_scan_state = 0; # Error
				last;
			    }
                        }
		    }
                }
            }
        }
    }
}
