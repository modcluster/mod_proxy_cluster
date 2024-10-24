# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestRequest 'GET';

# use Test::More;
use ModProxyCluster;

plan tests => 205;

Apache::TestRequest::module("mpc_test_host");
my $hostport = Apache::TestRequest::hostport();

my $url = "http://$hostport/";
my $resp = GET $url;

ok $resp->is_success;
ok (index($resp->as_string, "mod_cluster/2.0.0.Alpha1-SNAPSHOT") != -1);


##################
##### CONFIG #####
##################

$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare' );

ok $resp->is_success;
ok ($resp->content eq "");

$resp = GET "$url/mod_cluster_manager";

ok $resp->is_success;
ok (index($resp->as_string, "Node spare") != -1);

##################
##### STATUS #####
##################

$resp = CMD 'STATUS', $url, ( JVMRoute => 'spare' );

ok $resp->is_success;

my %p = parse_response 'CONFIG', $resp->content;

ok ($p{JVMRoute} eq 'spare');
ok ($p{Type} eq 'STATUS-RSP');
ok (exists $p{id});
ok (exists $p{State});


##################
#####  INFO  #####
##################

$resp = CMD 'INFO', $url;

ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Nodes}} == 2);
ok (@{$p{Contexts}} == 0);
ok (@{$p{Hosts}} == 0);

# There are two Nodes already, one `spare` added by us and `next` from the previous tests
## TODO: Make this implementation independent, i.e., we should not care whether indexing starts by 0 or 1...
ok ($p{Nodes}->[0]{Name} eq 'next');
ok ($p{Nodes}->[1]{Name} eq 'spare');

# All returned by INFO (+ Node: [%d])
my @info_opts = qw( Name Balancer LBGroup Host Port Type Flushpackets Flushwait Ping Smax Ttl Elected Read Transfered Connected Load );

for my $node (@{$p{Nodes}}) {
	ok (keys %$node == @info_opts + 1);
	for my $opt (@info_opts) {
		ok (exists $node->{$opt});
	}
}


##################
#####  DUMP  #####
##################

$resp = CMD 'DUMP', $url;

ok $resp->is_success;

%p = parse_response 'DUMP', $resp->content;

ok (@{$p{Balancers}} == 1);
ok (@{$p{Nodes}} == 2);
ok (@{$p{Contexts}} == 0);
ok (@{$p{Hosts}} == 0);

my @balancers = @{$p{Balancers}};

ok (@balancers == 1); # only 1 balancer

ok ($balancers[0]{Name} eq 'mycluster');
ok ($balancers[0]{Cookie} eq 'JSESSIONID'); # default value
ok ($balancers[0]{Path} eq 'jsessionid');   # default value

# All returned by DUMP/Balancer (+ list of nodes under Nodes key)
my @dump_balancer_opts = qw( Name Sticky remove force force Timeout maxAttempts );

for my $opt (@dump_balancer_opts) {
	ok (exists $balancers[0]->{$opt});
}

# All returned by DUMP/Node (+ node key)
my @dump_node_opts = qw( JVMRoute Balancer LBGroup Host Port Type flushpackets flushwait ping smax ttl timeout );

for my $node (@{$p{Nodes}}) {
	ok (keys %$node == @dump_node_opts + 1);
	for my $opt (@dump_node_opts) {
		ok (exists $node->{$opt});
	}
}


##################
##### STATUS #####
##################
my @status_opts = qw( Type JVMRoute State id );
	
for my $jvmroute ('next', 'spare') {
	$resp = CMD 'STATUS', $url, ( JVMRoute => $jvmroute );
	
	ok $resp->is_success;
	
	my %node = parse_response 'STATUS', $resp->content;

	ok (keys %node == @status_opts);
	ok ($node{JVMRoute} eq $jvmroute);
	ok ($node{Type} eq 'STATUS-RSP');

	for my $opt (@status_opts) {
		ok (exists $node{$opt});
	}
}

##################
#####  PING  #####
##################
my @ping_opts = qw( Type State id );

$resp = CMD 'PING', $url;

ok $resp->is_success;

%p = parse_response 'PING', $resp->content;

ok (keys %p == @ping_opts);
ok ($p{Type} eq 'PING-RSP');

for my $jvmroute ('next', 'spare') {
	$resp = CMD 'PING', $url, ( JVMRoute => $jvmroute );
	
	ok $resp->is_success;
	
	my %node = parse_response 'PING', $resp->content;

	ok (keys %node == @ping_opts + 1);
	ok ($node{JVMRoute} eq $jvmroute);
	ok ($node{Type} eq 'PING-RSP');

	for my $opt (@status_opts) {
		ok (exists $node{$opt});
	}
}

######################
##### ENABLE-APP #####
######################

# TODO: A better way..?
Apache::TestRequest::module("fake_cgi_app");
my $apphostport = Apache::TestRequest::hostport();
my ($apphost, $appport) = split ':', $apphostport;
Apache::TestRequest::module("mpc_test_host");

my $app = "http://$hostport/news";
$resp = GET $app;

ok $resp->is_error;

$resp = CMD 'CONFIG', $url, ( JVMRoute => 'app', Host => $apphost, Port => $appport, Type => 'http' );
ok $resp->is_success;


$resp = CMD 'ENABLE-APP', $url, ( JVMRoute => 'app', Context => '/news', Alias => $apphost );

ok $resp->is_success;
ok ($resp->content eq "");

$resp = GET $app;
ok $resp->is_success;


# TODO: The following test should work..?!
# ok (index($resp->as_string, "REDIRECT_") != -1);
ok (index($resp->as_string, "Fake App!") != -1);

# Check whether INFO knows about the app
$resp = CMD 'INFO', $url;

ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 1);
ok (@{$p{Hosts}} == 1);
ok ($p{Contexts}->[0]{Context} eq '/news');
ok ($p{Contexts}->[0]{Status} eq 'ENABLED');
ok ($p{Hosts}->[0]{Alias} eq $apphost);

# Check whether DUMP knows about the app
$resp = CMD 'DUMP', $url;

ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

ok (@{$p{Balancers}} == 1);
ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 1);
ok ($p{Contexts}->[0]{path} eq '/news');
ok ($p{Contexts}->[0]{status} == 1);
ok (@{$p{Hosts}} == 1);
ok ($p{Hosts}->[0]{alias} eq $apphost);
ok ($p{Hosts}->[0]{vhost} == $p{Contexts}->[0]{vhost});


#######################
##### DISABLE-APP #####
#######################

$resp = CMD 'DISABLE-APP', $url, ( JVMRoute => 'app', Context => '/news', Alias => $apphost );

ok $resp->is_success;
ok ($resp->content eq "");

$resp = GET $app;
ok $resp->is_error;

# Check whether INFO knows about the app
$resp = CMD 'INFO', $url;

ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 1);
ok (@{$p{Hosts}} == 1);
ok ($p{Contexts}->[0]{Context} eq '/news');
ok ($p{Contexts}->[0]{Status} eq 'DISABLED');
ok ($p{Hosts}->[0]{Alias} eq $apphost);

# Check whether DUMP knows about the app
$resp = CMD 'DUMP', $url;

ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

ok (@{$p{Balancers}} == 1);
ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 1);
ok ($p{Contexts}->[0]{path} eq '/news');
ok ($p{Contexts}->[0]{status} == 2);
ok (@{$p{Hosts}} == 1);
ok ($p{Hosts}->[0]{alias} eq $apphost);
ok ($p{Hosts}->[0]{vhost} == $p{Contexts}->[0]{vhost});


######################
#####  STOP-APP  #####
######################
CMD 'ENABLE-APP', $url, ( JVMRoute => 'app', Context => '/news', Alias => $apphost );

my @stop_opts = qw( Type JvmRoute Alias Context Requests );
$resp = CMD 'STOP-APP', $url, ( JVMRoute => 'app', Context => '/news', Alias => $apphost );

ok $resp->is_success;
%p = parse_response 'STOP-APP', $resp->content;

ok ($p{Type} eq 'STOP-APP-RSP');

for my $opt (@stop_opts) {
	ok (exists $p{$opt});
}

$resp = GET $app;
ok $resp->is_error;

# Check whether INFO knows about the app
$resp = CMD 'INFO', $url;

ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 1);
ok (@{$p{Hosts}} == 1);
ok ($p{Contexts}->[0]{Context} eq '/news');
ok ($p{Contexts}->[0]{Status} eq 'STOPPED');
ok ($p{Hosts}->[0]{Alias} eq $apphost);

# Check whether DUMP knows about the app
$resp = CMD 'DUMP', $url;

ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

ok (@{$p{Balancers}} == 1);
ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 1);
ok ($p{Contexts}->[0]{path} eq '/news');
ok ($p{Contexts}->[0]{status} == 3);
ok (@{$p{Hosts}} == 1);
ok ($p{Hosts}->[0]{alias} eq $apphost);
ok ($p{Hosts}->[0]{vhost} == $p{Contexts}->[0]{vhost});



######################
##### REMOVE-APP #####
######################
CMD 'ENABLE-APP', $url, ( JVMRoute => 'app', Context => '/news', Alias => $apphost );

$resp = CMD 'REMOVE-APP', $url, ( JVMRoute => 'app', Context => '/news', Alias => $apphost );

ok $resp->is_success;

ok ($resp->content eq "");

$resp = GET $app;
ok $resp->is_error;

# Check whether INFO knows about the app
$resp = CMD 'INFO', $url;

ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 0);
ok (@{$p{Hosts}} == 0);

# Check whether DUMP knows about the app
$resp = CMD 'DUMP', $url;

ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

ok (@{$p{Balancers}} == 1);
ok (@{$p{Nodes}} == 3);
ok (@{$p{Contexts}} == 0);
ok (@{$p{Hosts}} == 0);
