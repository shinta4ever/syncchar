#!/usr/bin/perl

# simics.pl - wrapper for simics command line options.  Avoids
# special-character escaping problems at TACC
#
# Maintainer: Don Porter <porterde@cs.utexas.edu>


use strict;
use warnings;

use Getopt::Long;

my $help;
my $freq_mhz = 1000;
my $num_cpus = 8;
my $benchmarks = "mab,killsim";
my $log_suffix = "osa_stat.log";
my $logdir = "log/";
my $grub_option = 0;
my $machines = 1;

my $result = GetOptions("help|h"            => \$help,
			"freq_mhz|f=i"      => \$freq_mhz,
			"num_cpus|n=i"      => \$num_cpus,
			"benchmarks|b=s"    => \$benchmarks,
			"log_suffix|l=s"    => \$log_suffix,
			"logdir=s"          => \$logdir,
			"grub_option|g=i"   => \$grub_option,
			"machines=i"        => \$machines,
			);
			
if($help){
    usage();
    exit;
}

my @benchmarks = split /,/, $benchmarks;
$benchmarks = join q/", "/, @benchmarks;
$benchmarks = "[\"$benchmarks\"]";

my $cmd = qq|./simics -stall -no-stc -e \'\$freq_mhz=$freq_mhz\' -e \'\$num_cpus = $num_cpus\' -e \'\@benchmarks = $benchmarks\' -e \'\@log_suffix = \"$log_suffix"\' -e \'\@log_dir = \"$logdir\"' -e \'\$grub_option = $grub_option\' -e \'\$machines = $machines\' osa-common.simics|;

exec($cmd);

sub usage{
    print "Usage: ./simics.pl [options]\n";
    print "\tfreq_mhz \n";
    print "\tnum_cpus  \n";
    print "\tbenchmarks\n";
    print "\tlog_suffix\n";
    print "\tlogdir  \n";
    print "\tmachines\n";
}
