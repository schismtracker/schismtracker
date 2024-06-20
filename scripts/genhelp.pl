#!/usr/bin/perl

use 5.8.0;
use strict;
use warnings;
use Encode;
use utf8;
use open ':encoding(utf-8)';

my $usage = sprintf("Usage: %s srcdir helptexts > helptext.c\n", $0);

if ($#ARGV < 1) {
	print STDERR $usage;
	exit 1;
}

foreach my $i (@ARGV) {
	if ($i eq "--help") {
		print STDOUT $usage;
		exit 0;
	}
}

my $srcdir = shift(@ARGV);
my @helptexts = @ARGV;

my $typechars = "|+:;!\%#=";

my @arrnames = ();

sub die_at {
	my ($filename, $line, $message) = @_;

	die(sprintf("%s:%d: %s\n", $filename, $line + 1, $message));
}

sub str_begins_with_spn {
	my ($str1, $str2, @others) = @_;

	foreach my $char (split('', $str2)) {
		if (rindex($str1, $char, 0) == 0) {
			return 1;
		}
	}

	return 0;
}

print "extern const unsigned char *help_text[];\n\n";

for my $i (0 .. $#helptexts) {
	my $file;
	open($file, $srcdir . "/" . $helptexts[$i]) or die(sprintf("could not open file %s", $srcdir . "/" . $helptexts[$i]));

	my $arrname = sprintf("help_text_%d", $i);

	printf("static const unsigned char %s[] = {\n", $arrname);

	my $blank = 1;
	while (my $line = <$file>) {
		$line =~ s/[\r\n]+//; # strip newline chars

		$blank = 0;

		if (length($line) <= 0) {
			# ignore empty lines
			next;
		} elsif (length($line) > 76) {
			die_at($helptexts[$i], $., "line is longer than 76 characters");
		} elsif ($line =~ / $/) {
			die_at($helptexts[$i], $., "trailing whitespace");
		} elsif (!str_begins_with_spn($line, $typechars)) {
			die_at($helptexts[$i], $., sprintf("line-type character %s is not one of %s", substr($line, 0, 1), $typechars))
		}

		foreach my $codepoint (split('', $line)) {
			$codepoint = ord($codepoint);

			if ($codepoint == 0x00B6) {
				$codepoint = 0x14;
			} elsif ($codepoint == 0x00A7) {
				$codepoint = 0x15;
			}

			printf("0x%02x, ", ord(encode("cp437", chr($codepoint))));
		}

		print("0x0A, \n");
	}

    if ($blank) {
        die_at($helptexts[$i], 0, "file is empty");
    }

    print("0x00};\n\n");
    $arrnames[$i] = $arrname;
}

print("const unsigned char* help_text[] = {\n");
foreach my $s (@arrnames) {
	printf("\t%s,\n", $s);
}
print("};\n");
