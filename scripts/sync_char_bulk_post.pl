#!/usr/bin/perl

use strict;
use warnings;

use Getopt::Long;

#only post-process files that are newer then their current post's
my $timestamp;
# Treat stamp a bit differently
my $stamp;

my $result = GetOptions("timestamp|t"          => \$timestamp,
                        "stamp|s"              => \$stamp,
    );

my $path = shift @ARGV;

my @stamp_bins = ('bayes.lock', 'genome.lock', 'intruder.lock[',
                  'kmeans.lock', 'labyrinth.lock', 'ssca2.lock',
                  'vacation.lock', 'yada.lock');

# Pre-pickle the kernel symbols so that the different processes don't
# clobber each other's pickle files
my @kernels;

if($stamp){
   foreach my $bin (@stamp_bins){
      $bin = "${path}/${bin}";
      push @kernels, $bin if(-e $bin);
   }
} else {
   @kernels = <${path}/vmlinux-2.6.16.1*>;
}

foreach my $kernel(@kernels){
   next if $kernel =~ /\.pkl$/; # Don't get pickled kernels
    `./sync_char_post.py -x ${kernel} -p ${kernel}.pkl -q`;
}

# Configurable number of processes to spread the work across
my $threads = 4;

my @workload = <${path}/syncchar\-*.log>;


if($timestamp){
   # If we selected timestmap mode, only post-process logs newer than
   # their post files

   my @newWorkload = ();

   foreach my $file(@workload){
      $file =~ /(.*)\.log$/;
      my $post = $1;
      $post .= '.post';

      if(-e $post){

         my ($def, $ino, $mode, $nlink, 
             $uid, $gid, $rdev, $size,
             $atime, $mtime, $ctime, $blksize, $blocks)
             = stat $file;

         my $log_mtime = $mtime;

         ($def, $ino, $mode, $nlink, 
          $uid, $gid, $rdev, $size,
          $atime, $mtime, $ctime, $blksize, $blocks)
             = stat $post;

         if($mtime >= $log_mtime && $size > 0){
            next;
         }
      } 
      push @newWorkload, $file;
   }

   # Replace workload with the pared down one
   @workload = @newWorkload;
}

my @pids;

# Fan out the work
my $increment = int(.5 + (scalar(@workload) / $threads));
$increment = 1 if $increment == 0;

for(my $index = 0; $index < scalar(@workload); $index += $increment){
   
   my $max = int($index + (scalar(@workload) / $threads));
   $max = $max > $#workload ? $#workload : $max;

	my @files = @workload[$index..$max];

	my $pid = fork;
	if($pid){
      # parent
      push @pids, $pid;

	} elsif(defined $pid){
      # child

      foreach my $file (@files){
         $file =~ /(.*)\.log$/;
         my $prefix = $1;

         if($stamp){ 
            
            $file =~ /(bayes|genome|intruder|kmeans|labyrinth|ssca2|vacation|yada)/ 
                or die "Don't know what do to with $file";

            my $kernel = $1;
            $kernel = "${path}/${kernel}.lock";

            `./sync_char_post.py -m -l $file -x ${kernel} -p ${kernel}.pkl > ${prefix}.post`;
            
         } else { # Linux

            $file =~ /.*\-([\d\.]+)-([\w\-]+)(?:\.osa\d+)?-?\d*\.log$/;
            
            my $kernel_version = $1;
            my $extra_version = $2;
            
            my $kernel = "${path}/vmlinux-${kernel_version}-${extra_version}";

            print `./sync_char_post.py -l $file -x ${kernel} -p ${kernel}.pkl > ${prefix}.post`;
         }
         
         if($? > 0){
            print "Error in $file\n";
         }
      }
      exit;

	} else {
      die "Can't fork: $!";
	}
}

# Wait on the chilluns
foreach my $cpid (@pids){
	waitpid $cpid, 0;
}


# Local variables:
#  perl-indent-level: 3
#  indent-tabs-mode: nil
#  tab-width: 3
# End:
#
# vim: ts=3 sw=3 expandtab
