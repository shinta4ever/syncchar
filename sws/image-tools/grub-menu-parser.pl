#!/usr/bin/perl

use strict;
use warnings;

my $counter = 0;
my $line;
open GRUB, ">../sws/grub.simics" or die "Can't open ../sws/grub.simics";

while($line = <STDIN>){
    if($line =~ /^kernel\s+(?:\/boot)?\/vmlinuz-([\w\.\-]+).*/){
	my $version = $1;
	print GRUB "\$system_map[$counter] = \"System.map-$version\"\n";
	print GRUB "\$sync_char_map[$counter] = \"sync_char.map.$version\"\n";
	print GRUB "\$sync_count_map[$counter] = \"sync_count.map.$version\"\n";
	$counter++;
    }
}

print GRUB <<EOF;

new-symtable
st0.plain-symbols \$system_map[\$grub_option]

EOF

close GRUB;
