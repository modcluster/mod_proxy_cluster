# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestRequest;

use ModProxyCluster;
Apache::TestRequest::module("mpc_test_host");

plan tests => 13, need_mpc;

my $hostport = Apache::TestRequest::hostport();

my $resp = CMD 'INFO';
ok $resp->is_success;
my %p = parse_response 'INFO', $resp->content;

my $host_count = scalar @{$p{Hosts}};

$resp = CMD 'CONFIG', { JVMRoute => 'modcluster824' };
ok $resp->is_success;

$resp = CMD 'ENABLE-APP', { JVMRoute => 'modcluster824', Context => '/news', Alias => "gamma,testalias" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok (@{$p{Hosts}} == $host_count + 2);

$resp = CMD 'ENABLE-APP', { JVMRoute => 'modcluster824', Context => '/news', Alias => "beta,testalias" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

# count should increase only by one
ok (@{$p{Hosts}} == $host_count + 3);

$resp = CMD 'ENABLE-APP', { JVMRoute => 'modcluster824', Context => '/news', Alias => "completely,unrelated" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

# two new vhosts should be created
ok (@{$p{Hosts}} == $host_count + 5);

# Clean after yourself
remove_nodes 'modcluster824';
sleep 25; # just to make sure we'll have enough time to get it removed

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok (@{$p{Hosts}} == $host_count);

END {
    remove_nodes 'modcluster824';
    sleep 25; # just to make sure we'll have enough time to get it removed
}

