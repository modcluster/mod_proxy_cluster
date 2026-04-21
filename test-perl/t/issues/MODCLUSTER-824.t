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

$resp = CMD 'CONFIG', { JVMRoute => 'modcluster824' };
ok $resp->is_success;

$resp = CMD 'ENABLE-APP', { JVMRoute => 'modcluster824', Context => '/news', Alias => "gamma,testalias" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Hosts}}, 2, "Two aliases are present");

$resp = CMD 'ENABLE-APP', { JVMRoute => 'modcluster824', Context => '/news', Alias => "beta,testalias" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

# count should increase only by one
my @ls = map { $_->{Alias} } @{$p{Hosts}};
ok t_cmp(@{$p{Hosts}}, 3, "A third alias was added (merged with the previous configuration: @ls)");

$resp = CMD 'ENABLE-APP', { JVMRoute => 'modcluster824', Context => '/news', Alias => "completely,unrelated" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

# two new vhosts should be created
@ls = map { $_->{Alias} } @{$p{Hosts}};
ok t_cmp(@{$p{Hosts}}, 5, "Another two aliases were added (@ls)");

# Clean after yourself
remove_nodes 'modcluster824';
sleep 25; # just to make sure we'll have enough time to get it removed

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Hosts}}, 0, "Everything is gone");

END {
    remove_nodes 'modcluster824';
    sleep 25; # just to make sure we'll have enough time to get it removed
}

