#!/usr/bin/perl
# pppEmu: arcade names list for the launcher, made from FBNeo's gamelist.txt (pfbneo.deps)
# usage: fbneo_arcade_list.pl <gamelist.txt> <output.tsv>
# output, one arcade romset per line (tab separated):
#   name  full name  parent  year  company  bios (1: bios / device rom, not a game)
use strict;
use warnings;

my ($in, $out) = @ARGV;
die "usage: $0 <gamelist.txt> <output.tsv>\n" unless $in && $out;
open(my $fi, '<', $in) or die "$in: $!\n";
open(my $fo, '>', $out) or die "$out: $!\n";

# console sets FBNeo also runs (not arcade games)
my $consoles = qr/^(spec|msx|sg1k|nes|fds|pce|tg|sgx|md|sms|gg|cv|chf|ngp|snes)_/;
my $count = 0;
while (my $line = <$fi>) {
    next unless $line =~ /^\|/;
    my @f = map { s/^\s+|\s+$//gr } split(/\|/, $line);
    # @f: '', name, status, full name, parent, year, company, hardware, remarks
    my ($name, $status, $full, $parent, $year, $company) = @f[1 .. 6];
    next if !defined $company || $name eq 'name';
    next if $name =~ $consoles;
    my $bios = ($full =~ /BIOS|Internal ROM/i || $full =~ /^Neo Geo( MVS system)?$/) ? 1 : 0;
    $company = '' if $company eq 'unknown';
    print $fo join("\t", $name, $full, $parent, $year, $company, $bios), "\n";
    $count++;
}
close($fi);
close($fo);
print "fbneo_arcade_list: $count arcade sets\n";
