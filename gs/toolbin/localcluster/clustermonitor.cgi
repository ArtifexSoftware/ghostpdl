#!/usr/bin/perl

use strict;
use warnings;

use CGI;

use Time::localtime;
use File::stat;

my $clusteroot = '/home/regression/cluster';

my $watchdog=0;

sub http_send {
  my $msg = shift;
  
  #print "HTTP/1.0 200 OK\r\n";
  print "Date: ", scalar gmtime, "\r\n";
  print "Content-Length: ", length $msg, "\r\n";
  #print "Content-Type: text/plain\r\n";
  #print "Content-Type: text/html\r\n";
  print "Content-Type: application/json\r\n";
  print "\r\n";
  print $msg;
}

sub get_status {
  my %status;

  #my @machines=("i7","i7a","macpro","peeves","test");
  my @machines = <$clusteroot/*.status>;
  #print "@machines\n";

  for (my $i=0;  $i<scalar(@machines);  $i++) {
    $machines[$i]=~s/.status//;
    $machines[$i]=~s|.*cluster/||;
  }
  #print "@machines\n";

  my $status=`cat $clusteroot/status`;
  chomp $status;

  if (!exists $status{'main'}{"status"} || $status{'main'}{"status"} ne $status) {
    $status{'main'}{"status"}=$status;
  }

  foreach my $machine (@machines) {
    my $s0=`cat $clusteroot/$machine.status`;
    chomp $s0;

    my $down="";
    my $downTime=0;
    $down="--DOWN--" if (!stat("$clusteroot/$machine.up") || 
      ($downTime=(time-stat("$clusteroot/$machine.up")->ctime))>300);

    if (!exists $status{$machine}{"status"} || 
         $status{$machine}{"status"} ne $s0 ||
         $status{$machine}{"down"} ne $down) {
      $status{$machine}{"status"}=$s0;
      $status{$machine}{"down"}=$down;
    }
  }

  my @jobs = `cat $clusteroot/queue.lst`;
  for (my $i = 0; $i < scalar(@jobs); $i++) {
    $status{'pending'}{$i} = $jobs[$i];
    chomp $status{'pending'}{$i};
  }

  return %status;
}

sub get_reports {
  my @reports;
  my @emails = <$clusteroot/archive/*/email.txt>;

  foreach my $email (@emails) {
    $email =~ s/.*(\d{5}-\d{5}).*/$1/;
    push @reports, $email;
  }

  return @reports;
}

# if we've been asked for a specific result, just return it
my $report = CGI::param('report');

if ($report =~ /^\d+-\d+$/) {
  print CGI::header('text/plain');
  #print open "$clusteroot/archive/$report/email.txt";
  print "report $report.\n\n";
  open my $file, "$clusteroot/archive/$report/email.txt";
  print while <$file>;
  close $file;
  exit;
}

# otherwise, send the dashboard json
my %status = get_status;
my @reports = get_reports;

# FIXME: need to escape the strings we encode as json
my $json = "";
$json .= "{\n";
$json .= "  \"status\" : \"$status{'main'}{'status'}\",\n";
$json .= "  \"nodes\"  : [\n";
foreach my $machine (sort keys %status) {
  next if $machine eq 'main';
  next if $machine eq 'pending';
  $json .= "   {\n";
  $json .= "    \"name\" : \"$machine\",\n";
  $json .= "    \"status\" : \"".$status{$machine}{'status'}."\",\n";
  $json .= "    \"down\"   : \"".$status{$machine}{'down'}."\"\n";
  $json .= "   },\n";
}
$json .= "  ],\n";
$json .= "  \"pending\" : [\n";
foreach my $job (sort keys %{$status{'pending'}}) {
  $json .= "  \"".$status{'pending'}{$job}."\",\n";
}
$json .= "  ],\n";
$json .= "  \"reports\" : [\n";
foreach my $report (@reports) {
  $json .= "  \"".$report."\",\n";
}
$json .= "  ]\n";
$json .= "}\n";

print CGI::header('application/json');
print $json;
#http_send $json;

