# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestRequest 'GET';

use ModProxyCluster;

Apache::TestRequest::module("fake_cgi_app");
my ($apphost, $appport) = split ':', Apache::TestRequest::hostport();
Apache::TestRequest::module("mpc_test_host");

plan tests => 47, need_mpc;


# DUMP with no nodes 
my $resp = CMD 'DUMP';

ok $resp->is_success;
ok t_cmp($resp->content, "");
my %p = parse_response 'DUMP', $resp->content;

# There might be a balancer in case of previously run tests (removal of all nodes keep balancers in, currently)
ok t_cmp(@{$p{Balancers}}, 0, "No balancer in DUMP output for empty cluster");
ok t_cmp(@{$p{Nodes}},     0, "No nodes in DUMP output for empty cluster");
ok t_cmp(@{$p{Contexts}},  0, "No contexts in DUMP output for empty cluster");
ok t_cmp(@{$p{Hosts}},     0, "No hosts in DUMP output for empty cluster");

# Now let's add a node
$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Scheme => "http", Port => $appport, Host => $apphost };
ok $resp->is_success;

$resp = CMD 'DUMP';
ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

ok t_cmp(@{$p{Balancers}}, 1, "The default balancer in DUMP output");
ok t_cmp(@{$p{Nodes}},     1, "Single node in DUMP output for empty cluster");
ok t_cmp(@{$p{Contexts}},  0, "No contexts in DUMP output for empty cluster");
ok t_cmp(@{$p{Hosts}},     0, "No hosts in DUMP output for empty cluster");

ok t_cmp($p{Nodes}->[0]{JVMRoute}, "fake-app");

# Add Context + Alias
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/context", Alias => "myalias" };
ok $resp->is_success;

$resp = CMD 'DUMP';
ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

ok t_cmp(@{$p{Balancers}}, 1, "The default balancer in DUMP output");
ok t_cmp(@{$p{Nodes}},     1, "Single nodes in DUMP output for empty cluster");
ok t_cmp(@{$p{Contexts}},  1, "Single context in DUMP output for empty cluster");
ok t_cmp(@{$p{Hosts}},     1, "Single host in DUMP output for empty cluster");

ok t_cmp($p{Contexts}->[0]{path}, "/context");
ok t_cmp($p{Hosts}->[0]{alias}, "myalias");

# Check that we are not missing any parameters
# 1) balancers
my @balancer_dump = qw( balancer Name Sticky remove Timeout maxAttempts );
foreach my $opt (@balancer_dump) {
    ok (exists $p{Balancers}->[0]{$opt});
}

# 2) nodes
my @node_dump = qw( node Balancer JVMRoute LBGroup Host Port Type flushpackets flushwait ping smax ttl timeout );
foreach my $opt (@node_dump) {
    ok (exists $p{Nodes}->[0]{$opt});
}

# 3) hosts
my @host_dump = qw( host vhost node );
foreach my $opt (@host_dump) {
    ok (exists $p{Hosts}->[0]{$opt});
}

# 4) contexts
my @context_dump = qw( context vhost node status );
foreach my $opt (@context_dump) {
    ok (exists $p{Contexts}->[0]{$opt});
}

# Clean after yourself by a simple restart of the server
END {
    my $ret = $?;
    my $cfg = Apache::Test::config();
    my $server = $cfg->server;
    $server->stop();
    $server->start();
    $? = $ret;
}

