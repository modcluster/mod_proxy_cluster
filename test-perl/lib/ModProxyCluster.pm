package ModProxyCluster;

use v5.32;

require Exporter;
use HTTP::Request;
use HTTP::Request::Common;
use LWP::UserAgent;

our @ISA = qw(Exporter);

our @EXPORT = qw(
  CMD
  parse_params
  parse_response
);

our $VERSION = '0.0.1';

sub CMD_internal {
	my ($cmd, $url, $params) = @_;
	my $header = [];

	my $ua = LWP::UserAgent->new();
	my $request = HTTP::Request->new($cmd, $url, $header, $params);

	return $ua->request($request);
}

sub concat_params {
	my (%params) = @_;
	my $p = "";
	my $d = "";

	foreach my $k (sort(keys %params)) {
		if ($params{$k}) {
			$p .= $d . $k . '=' . $params{$k};
			$d = "&";
		}
	}

	return $p;
}

sub parse_two_layers {
	my ($d1, $d2, $string) = @_;

	my %params;
	for my $pair (split $d1, $string) {
		my ($key, $val) = split $d2, $pair;
		# Needed because of the leading whitespace before Type field in DUMP/node...
		# TODO: Remove this after we fix the issue...
		(my $fixkey = $key) =~ s/\s+//g;
		$params{$fixkey} = $val;
	}

	return %params;

}

sub parse_params {
	return parse_two_layers '&', '=', @_;
}

sub parse_INFO {
	my $input = shift;
	my @lines = split '\n', $input;
	my (@nodes, @hosts, @contexts);

	my $line = "";
	my $i = 0;
	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/Node:/) {
			last;
		}
		my %node = parse_two_layers ',', ': ', $line;
		push @nodes, \%node;
	}

	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/Vhost:/) {
			last;
		}
		my %host = parse_two_layers ',', ': ', $line;
		push @hosts, \%host;
	}

	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/Context:/) {
			last;
		}
		my %context = parse_two_layers ',', ': ', $line;
		push @contexts, \%context;
	}

	# Check that everything was parsed
	return () if $i != @lines;
	return (Nodes => \@nodes, Hosts => \@hosts, Contexts => \@contexts );
}

sub parse_DUMP {
	my $input = shift;
	my @lines = split '\n', $input;
	my (@balancers, @nodes, @hosts, @contexts);

	my $line = "";
	my $i = 0;
	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/^balancer:/) {
			last
		}
		(my $b = $line) =~ s/: /=/g;
		$b =~ s/\[([^\]]*)\]\/\[([^\]]*)\]/Cookie=$1 Path=$2/;
		my %balancer = parse_two_layers ' ', '=', $b;
		push @balancers, \%balancer;
	}

	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/^node:/) {
			last;
		}
		(my $n = $line) =~ s/LBGroup: \[([^\]]*)\]/LBGroup: $1/;
		my %node = parse_two_layers ',', ': ', $n;
		push @nodes, \%node;
	}

	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/^host:/) {
			last;
		}
		(my $h = $line) =~ s/: /=/g;
		$h =~ s/\[([^\]]*)\]/alias=$1/;
		my %host = parse_two_layers ' ', '=', $h;
		push @hosts, \%host;
	}


	for (; $i < @lines; $i++) {
		$line = $lines[$i];
		if ($line !~ m/^context:/) {
			last;
		}
		(my $c = $line) =~ s/: /=/g;
		$c =~ s/\[([^\]]*)\]/path=$1/;
		my %context = parse_two_layers ' ', '=', $c;
		push @contexts, \%context;
	}

	# Check that the whole input was consumed!
	return () if $i != @lines;
	return ( Balancers => \@balancers, Nodes => \@nodes, Hosts => \@hosts, Contexts => \@contexts );
}

sub CMD {
	my ($cmd, $url, %params) = @_;
	my @mpc_commands = qw(CONFIG ENABLE-APP DISABLE-APP STOP-APP REMOVE-APP STOP-APP-RSP
				STATUS STATUS-RSP INFO INFO-RSP DUMP DUMP-RSP PING PING-RSP);

	if (grep /^$cmd$/, @mpc_commands) {
		return CMD_internal $cmd, $url, concat_params %params;
	}

	return HTTP::Response->new();
}

sub parse_response {
	my ($cmd, $resp) = @_;

	if ($cmd eq 'CONFIG') {
		return parse_params $resp;
	} elsif ($cmd eq 'DUMP') {
		return parse_DUMP $resp;
	} elsif ($cmd eq 'INFO') {
		return parse_INFO $resp;
	} elsif ($cmd eq 'STATUS') {
		return parse_params $resp;
	} elsif ($cmd eq 'PING') {
		return parse_params $resp;
	} elsif ($cmd eq 'STOP-APP') {
		return parse_params $resp;
	}

	return {};
}


1;

