#!/usr/bin/perl
#search HvacFurnaceNN.log for short cycling the compressor
use strict;
use warnings;

use DateTime::Format::Strptime;

my $parse = DateTime::Format::Strptime->new(
pattern => '%T',
on_error => 'croak',
);

my $linenum = 1;
my $prevdt;
my $prevval;
my $prevprevval;
my $prevline;
my $prevprevline;
my $prevprevdt;
foreach my $line ( <STDIN> ) {
    chomp( $line );
    my @fields = split ' ', $line;
    my $dt = $parse->parse_datetime($fields[1]);
    my $val = $fields[7];
     $linenum += 1;
     if (defined( $prevprevdt))  {
        my $dseconds = $dt->subtract_datetime_absolute($prevprevdt)->seconds;

        if ($val == 19 && $prevval <= 1 && $prevprevval > 1 && $dseconds < 250)
        {
            print "*****\n";
            print $prevprevline . "\n";
            print $prevline . "\n";
            print $line . "\n";
        }
    }
    $prevprevdt = $prevdt;
    $prevdt = $dt;
    $prevprevval = $prevval;
    $prevval = $val;
    $prevprevline = $prevline;
    $prevline = $line;
    
}
