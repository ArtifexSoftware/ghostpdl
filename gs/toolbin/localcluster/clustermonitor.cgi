#!/usr/bin/perl

use strict;
use warnings;

use Time::localtime;
use File::stat;


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
  my @machines = </home/marcos/cluster/*.status>;
  #print "@machines\n";

  for (my $i=0;  $i<scalar(@machines);  $i++) {
    $machines[$i]=~s/.status//;
    $machines[$i]=~s|.*cluster/||;
  }
  #print "@machines\n";

  my $status=`cat /home/marcos/cluster/status`;
  chomp $status;

  if (!exists $status{'main'}{"status"} || $status{'main'}{"status"} ne $status) {
    $status{'main'}{"status"}=$status;
  }

  foreach my $machine (@machines) {
    my $s0=`cat /home/marcos/cluster/$machine.status`;
    chomp $s0;

    my $down="";
    my $downTime=0;
    $down="--DOWN--" if (!stat("/home/marcos/cluster/$machine.up") || 
      ($downTime=(time-stat("/home/marcos/cluster/$machine.up")->ctime))>300);

    if (!exists $status{$machine}{"status"} || 
         $status{$machine}{"status"} ne $s0 ||
         $status{$machine}{"down"} ne $down) {
      $status{$machine}{"status"}=$s0;
      $status{$machine}{"down"}=$down;
    }
  }

  my @jobs = `cat /home/marcos/cluster/user.run`;
  for (my $i = 0; $i < scalar(@jobs); $i++) {
    $status{'pending'}{$i} = $jobs[$i];
    chomp $status{'pending'}{$i};
  }

  return %status;
}


my %status = get_status;

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
$json .= "  ]\n";
$json .= "}\n";

http_send $json;

