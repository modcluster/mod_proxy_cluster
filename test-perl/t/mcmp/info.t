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

# get the fake cgi app host and port
Apache::TestRequest::module("fake_cgi_app");
my ($apphost, $appport) = split ':', Apache::TestRequest::hostport();
Apache::TestRequest::module("mpc_test_host");

plan tests => 40, need_mpc;


my $resp = CMD 'INFO';
ok $resp->is_success;
ok t_cmp($resp->content, "", "INFO for an empty cluster should be empty");

# check that the parse_response is empty too
my %p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Nodes}}, 0, "Empty cluster has no Nodes");
ok t_cmp(@{$p{Contexts}}, 0, "Empty cluster has no Contexts");
ok t_cmp(@{$p{Hosts}}, 0, "Empty cluster has no Aliases");

# Now let's add a node
$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Scheme => "http", Port => $appport, Host => $apphost };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "CONFIG added one node");
ok t_cmp(@{$p{Contexts}}, 0, "No Contexts were added");
ok t_cmp(@{$p{Hosts}}, 0, "No Aliases were added");

ok t_cmp($p{Nodes}->[0]{Name}, "fake-app");

# Check only that the values are present, we should check defaults elsewhere
my @node_info = qw( Node Name Balancer LBGroup Host Port Type Flushpackets Flushwait Ping Smax Ttl Elected Read Transfered Connected Load );
foreach my $opt (@node_info) {
    ok (exists $p{Nodes}->[0]{$opt});
}

# Add Context + Alias
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/context", Alias => "myalias" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Nodes}} == 1);
ok (@{$p{Contexts}} == 1);
ok (@{$p{Hosts}} == 1);
ok t_cmp($p{Nodes}->[0]{Name}, "fake-app");
ok t_cmp($p{Contexts}->[0]{Context}, "/context");
ok t_cmp($p{Hosts}->[0]{Alias}, "myalias");

# Check again Context's and Alias' properties
my @alias_info = qw( Vhost Alias );
foreach my $opt (@alias_info) {
    ok (exists $p{Hosts}->[0]{$opt});
}

# TODO: In reality, INFO sends 3, but Context is there TWICE!
my @context_info = qw( Context Status );
foreach my $opt (@context_info) {
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

