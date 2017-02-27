#!/usr/bin/perl
#example usage:  perl ./make-tileset.pl foo 00 tiles/generic.png --solid >> foo.cfg


use strict;

die "usage: $0 <tilename> <base> <image>" if @ARGV < 3;

open DATE, "date |" or die;
my $date = '';
while(my $line = <DATE>) {
	chomp $line;
	$date .= $line;
}
close DATE;

my $command_line = "$0 " . (join ' ', @ARGV);
print "# Generated on $date using:\n#  $command_line\n";

print "{\n";

my $tilename = shift @ARGV;
my $base = shift @ARGV;
my $image = shift @ARGV;

$tilename = "($tilename)" if $tilename =~ /[a-zA-Z0-9]+/;

my $solid = '';
my $grass = '';
my $friend = $tilename;


while(my $arg = shift @ARGV) {
	if($arg eq '--solid') {
		$solid = "solid: true,";
	} elsif($arg eq '--no-solid') {
		$solid = "solid: false,";
	} elsif($arg eq '--friend') {
		$friend = shift @ARGV;
	} else {
		die "Unrecognized argument: $arg";
	}
}

while((length $tilename) < 3) {
	$tilename = " $tilename";
}

print "tile_pattern: [\n";

printf qq~
	#horizontal tile
	{
		image:"$image",
		tiles:"%s",
		%s
		pattern:"
		.* ,   ,.*  ,
		$friend,$tilename,$friend,
		.* ,   ,.* "
	},
~, &coord($base, 3, 1), $solid ;

printf qq~
	#horizontal tile with one tile below but not on either side
	{
		image:"$image",
		tiles:"%s",
		%s
		pattern:"
		.* ,   ,.*  ,
		$friend,$tilename,$friend,
		$friend?,$friend,$friend?"
	},
~, &coord($base, 0, 2), $solid;

printf qq~
	#horizontal tile with one tile above but not on either side
	{
		image:"$image",
		tiles:"%s",
		%s
		pattern:"
		$friend?,$friend,$friend?,
		$friend,$tilename,$friend,
		.* ,   , .*"
	},
~, &coord($base, 2, 2), $solid;

printf qq~
	#overhang
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		%s
		pattern:"
		.* ,   ,.*  ,
		   ,$tilename,$friend,
		.* ,   ,.* "
	},
~, &coord($base, 3, 0), $solid;

printf qq~
	#overhang - reversed
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		%s
		pattern:"
		.* ,   ,.*  ,
		$friend,$tilename,   ,
		.* ,   ,.* "
	},
~, &coord($base, 3, 2), $solid;


printf qq~
	#single tile by itself
	{
		image:"$image",
		tiles:"%s",
		%s
		pattern:"
		 .*,   , .*,
		   ,$tilename,   ,
		 .*,   , .*"
	},
~, &coord($base, 3, 3), $solid;

printf qq~
	#top of thin platform
	{
		image:"$image",
		tiles:"%s",
		%s
		pattern:"
		 .*,   , .*,
		   ,$tilename,   ,
		 .*,$friend, .*"
	},
~, &coord($base, 0, 0), $solid;

printf qq~
	#part of thin platform
	{
		image:"$image",
		tiles:"%s",
		$solid
		pattern:"
		 .*,$friend, .*,
		   ,$tilename,   ,
		 .*,$friend, .*"
	},
~, &coord($base, 1, 0);

printf qq~
	#bottom of thin platform
	{
		image:"$image",
		tiles:"%s",
		$solid
		pattern:"
		 .*,$friend, .*,
		   ,$tilename,   ,
		 .*,   , .*"
	},
~, &coord($base, 2, 0);


printf qq~
	#cliff edge -- version with a corner underneath/opposite
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		%s
		pattern:"
		  .*,   ,$friend?,
			,$tilename,$friend ,
		$friend?,$friend,$friend?"
	},
~, &coord($base, 0, 3), $solid;

printf qq~
	#cliff edge (reversed) -- version with a corner underneath/opposite
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		%s
		pattern:"
		$friend?,   ,.*,
		$friend,$tilename,   ,
		$friend?,$friend,$friend?"
	},
~, &coord($base, 0, 1), $solid;

printf qq~
	#middle of a cross
	{
		image:"$image",
		tiles:"%s",
		$solid
		pattern:"
		$friend?,$friend,$friend?,
		$friend,$tilename,$friend ,
		$friend?,$friend,$friend?"
	},
~, &coord($base, 1, 2);




printf qq~
	#bottom corner with corner on opposite side
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		$solid
		pattern:"
		$friend?,$friend,$friend?,
			,$tilename,$friend,
		.*  ,   , .*"
	},
~, &coord($base, 2, 3);

printf qq~
	#bottom corner with corner on opposite side (reversed)
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		$solid
		pattern:"
		$friend?,$friend,$friend?,
		$friend,$tilename,   ,
		.*  ,   , .*"
	},
~, &coord($base, 2, 1);


printf qq~
	#cliff face coming both up and down from a one-tile thick cliff and expanding
	#out into a ledge in one direction
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		$solid
		pattern:"
		$friend?,$friend,.* ,
		$friend,$tilename,   ,
		$friend?,$friend,.* "
	},
~, &coord($base, 1, 1);

printf qq~
	#cliff face coming both up and down from a one-tile thick cliff and expanding
	#out into a ledge in one direction (reversed)
	{
		image:"$image",
		reverse:false,
		tiles:"%s",
		$solid
		pattern:"
		.* ,$friend,$friend?,
		   ,$tilename,$friend,
		.* ,$friend,$friend?"
	},
~, &coord($base, 1, 3);



sub base_unencode($) {
	my $base = lc(shift @_);
	$base = 10 + ord($base) - ord('a') if ord($base) >= ord('a') and ord($base) <= ord('z');
	return $base;
}

sub base_encode($) {
	my $val = shift @_;
	$val = chr(ord('a') + ($val - 10)) if $val >= 10;
	return $val;
}

sub coord($$$) {
	my ($base, $row, $col) = @_;
	my @base = split //, $base;
	$row += &base_unencode($base[0]);
	$col += &base_unencode($base[1]);

	return &base_encode($row) . &base_encode($col);
}

print "]\n}\n";
