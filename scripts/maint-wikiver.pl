#!/usr/bin/perl
use LWP::UserAgent;
my $ua = LWP::UserAgent->new();
push @{ $ua->requests_redirectable }, 'POST'; # apparently mediawiki didn't read RFC 2616

my $r = $ua->get('http://sovietrussia.org/mediawiki/index.php?title=Template:CVSVersionDate&action=edit');

die "$@" unless ($r->is_success());
my $x = $r->content();
my %h;
foreach my $y ($x =~ m#<input(?:[^>]*?)type=(?:"hidden"|'hidden'|hidden)\s*(?:[^>]*)>#gis) {
	my $k = ($y =~ /name=(?:'([^']*)'|"([^"]*)"|(\S+))/i) ? ($1 || $2 || $3) : next;
	my $v = ($y =~ /value=(?:'([^']*)'|"([^"]*)"|(\S+))/i) ? ($1 || $2 || $3) : next;
	$h{$k}=$v;
}
my $wn=join("",($x=~m#<textarea(?:[^>]*?)name=(?:'wpTextbox1'|"wpTextbox1"|wpTextbox1)[^>]*>([^<]*)<#sgi));
$wn =~ s/^\s+//s;$wn =~ s/\s+$//s;
my @tm = gmtime();
my $now = sprintf("%02d %s %04d",
		$tm[3],
		+[qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec)]->[ $tm[4] ],
		($tm[5]+1900));
if ($now eq $wn) {
	print "Already built today\n";
	exit 0;
}
$h{wpSave} = "Save page";
$h{wpSummary} = "Automatic update: $now";
$h{wpTextbox1} = $now;
$r = $ua->post('http://sovietrussia.org/mediawiki/index.php?title=Template:CVSVersionDate&action=submit', \%h);
die $r->as_string() unless ($r->is_success());
print "Updated sovietrussia.org\n";
exit 0;
