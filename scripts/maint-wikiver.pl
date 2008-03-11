#!/usr/bin/perl
use LWP::UserAgent;
my $ua = LWP::UserAgent->new(cookie_jar => {});
push @{ $ua->requests_redirectable }, 'POST'; # apparently mediawiki didn't read RFC 2616

my $r = $ua->get('http://sovietrussia.org/mediawiki/index.php?title=Template:CVSVersionDate&action=edit');

die "$@" unless ($r->is_success());
my $x = $r->content();
my %h;
foreach my $y ($x =~ m#<input(?:[^>]*?)type=(?:"text"|'text'|text)\s*(?:[^>]*)>#gis) {
	my $k = ($y =~ /name\s*=\s*(?:'([^']*)'|"([^"]*)"|(\S+))/si) ? ($1 || $2 || $3) : next;
	my $v = ($y =~ /value\s*=\s*(?:'([^']*)'|"([^"]*)"|(\S+))/si) ? ($1 || $2 || $3) : next;
	$h{$k}=$v||'';
}
foreach my $y ($x =~ m#<input(?:[^>]*?)type=(?:"hidden"|'hidden'|hidden)\s*(?:[^>]*)>#gis) {
	my $k = ($y =~ /name\s*=\s*(?:'([^']*)'|"([^"]*)"|(\S+))/si) ? ($1 || $2 || $3) : next;
	my $v = ($y =~ /value\s*=\s*(?:'([^']*)'|"([^"]*)"|(\S+))/si) ? ($1 || $2 || $3) : next;
	$h{$k}=$v||'';
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
delete $h{search};
my $url = URI->new_abs('/mediawiki/index.php?title=Template:CVSVersionDate&action=submit',
		$r->base);
$r = $ua->post($url, \%h);
die $r->as_string() unless ($r->is_success());
print "Updated " . $url->host . "\n";
exit 0;
