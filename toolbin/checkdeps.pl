#!/usr/bin/perl
#
# A somewhat specialised dependency checker; checks the contents of
# base and lib.mak to ensure that lib.mak properly lists all the
# dependencies for each .h and .c file.

use List::MoreUtils qw(uniq);

sub gather_includes {
    my ($dir) = @_;

    # First, gather all the #includes.
    my @rawincludes=`cd $dir;grep include *.h *.c`;

    # Extract just the #includes that start on a line, and aren't system
    # ones.
    my %include=();
    foreach $inc (@rawincludes) {
        if ((my $a, my $b) = $inc =~ /(\S+):\s*#\s*include\s*\"(\S+)\"/) {
            if (!exists $include{$a}) {
                $include{$a} = [];
            }
            $b =~ s/(\S+)\.h/\$\($1_h\)/;
            #print "$a:$b\n";
            push @{ $include{$a} }, $b;
        }
    }
    return %include;
}

sub read_makefile {
    my ($dir, $file) = @_;

    # Now, run through lib.mak
    my %depend=();
    my %vars=();
    open(IN, "$dir/$file") or die "can't open $dir/$file";
    LINE: while (<IN>) {
        # Reassemble split lines
        $_ =~ s/[\r\n]*$//;
        my $line=$_;
        while (substr($line,-1) eq "\\") {
            my $nextline = <IN>;
            $nextline =~ s/[\r\n]*$//;
            substr($line,-1) = $nextline;
        }

        # Now, we're interested in 2 classes of lines.
        # Firstly those of the form:       file.blah : [dependency | symbol] *
        if ((my $f, my $deps) = $line =~ /^([A-Za-z0-9_\$\(\)\.]+)\s*:\s*(.*)/) {
            next LINE if (@deps eq "");
            my @deplist = split /\s+/, $deps;
            if (!exists $depend{$f}) {
                $depend{$f} = ();
            }
            push @{ $depend{$f} }, @deplist;
        }
        # And secondly, those of the form: something = filelist
        elsif ((my $var, $val) = $line =~ /^([A-Za-z0-9_]+)\s*=\s*(.*)/) {
            next LINE if (@val eq "");
            my @values = split /\s+/, $val;
            if (!exists $vars{$var}) {
                $vars{$var} = [];
            }
            push @{ $vars{$var} }, @values;
        }
    }
    close(IN);
    return (\%depend, \%vars);
}

sub check_vars
{
    (my $include_ref, my $vars_ref) = @_;
    my %include = %$include_ref;
    my %vars    = %$vars_ref;
    # Now, check the vars
    VAR: foreach $var (sort keys %vars) {
        # We only care about vars of the form: blah_h
        next VAR if !((my $v) = $var =~ /(\w+)_h/);
        my @inclist = ( "\$(GLSRC)$v.h" );
        if (exists $include{"$v.h"}) {
            push @inclist, @ { $include{"$v.h"} };
        }
        @inclist = uniq(sort @inclist);
        @deplist = sort @ { $vars{$var} };
        # Compare the dependency lists
        #print "DEP=".join(', ', @deplist)."\n";
        #print "INC=".join(', ', @inclist)."\n";
        $a = shift @deplist;
        $b = shift @inclist;
        while (defined($a) || defined($b)) {
            #print ("comparing $a and $b\n");

            # We are too crap to properly know the paths of things, so fudge it
            if (defined($a) && defined($b) && $a ne $b &&
                ($a =~ m/\$\(.*\)(\S+).h/) &&
                ($b =~ m/\$\(.*\)$1.h/))
            {
                print "$v.h: accepting $a ($b expected)\n";
                $a = shift @deplist;
                $b = shift @inclist;
            }
            elsif (defined($a) && ($a lt $b || !defined($b))) {
                print "$v.h: extra dependency on $a\n";
                $a = shift @deplist;
            } elsif (defined($b) && ($a gt $b || !defined($a))) {
                print "$v.h: missing dependency on $b\n";
                $b = shift @inclist;
            } else {
                $a = shift @deplist;
                $b = shift @inclist;
            }
        }
        #print "==\n";
    }
}

sub check_depend
{
    (my $include_ref, my $depend_ref) = @_;
    my %include = %$include_ref;
    my %depend  = %$depend_ref;
    # Now, check the depends
    VAR: foreach $dep (sort keys %depend) {
        if ((my $path,my $d) = $dep =~ /^(\S+?)([a-zA-Z0-9_]+)\.\$\(OBJ\)/) {
            # $(GLOBJ)file.$(OBJ)
            my @inclist = ( "\$(GLSRC)$d.c" );
            if (exists $include{"$d.c"}) {
                push @inclist, @ { $include{"$d.c"} };
            }
            @inclist = uniq(sort @inclist);
            @deplist = sort @ { $depend{$dep} };
            # Compare the dependency lists
            #print "DEP=".join(', ', @deplist)."\n";
            #print "INC=".join(', ', @inclist)."\n";
            $a = shift @deplist;
            $b = shift @inclist;
            while (defined($a) || defined($b)) {
                #print ("comparing $a and $b\n");

                # We are too crap to properly know the paths, so fudge it
                if (defined($a) && defined($b) && $a ne $b &&
                    ($a =~ m/\$\(.*\)(\S+).h/) &&
                    ($b =~ m/\$\(.*\)$1.h/))
                {
                    print "$d.\$(OBJ): accepting $a ($b expected)\n";
                    $a = shift @deplist;
                    $b = shift @inclist;
                }
                elsif ($a eq "\$(AK)") {
                    # Silently accept the AKs
                    $a = shift @deplist;
                }
                elsif ($a eq "STDDIRS") {
                    # Silently accept the STDDIRSs (should probably insist on
                    # these!)
                    $a = shift @deplist;
                }
                elsif (defined($a) && ($a lt $b || !defined($b))) {
                    print "$d.\$(OBJ): extra dependency on $a\n";
                    $a = shift @deplist;
                } elsif (defined($b) && ($a gt $b || !defined($a))) {
                    print "$d.\$(OBJ): missing dependency on $b\n";
                    $b = shift @inclist;
                } else {
                    $a = shift @deplist;
                    $b = shift @inclist;
                }
            }
            #print "==\n";
        }
    }
}

# Main.

my %include, %depend, %vars;
my $depend_ref, $vars_ref;

%include                 = gather_includes("base");
($depend_ref, $vars_ref) = read_makefile("base", "lib.mak");
%depend = %$depend_ref;
%vars   = %$vars_ref;
check_vars(\%include, \%vars);
check_depend(\%include, \%depend);
