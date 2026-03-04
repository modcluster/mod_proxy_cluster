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

Apache::TestRequest::module("mpc_test_host");

plan tests => 29, need_mpc;

################################
### Check the default values ###
################################
my $resp = CMD 'CONFIG', { JVMRoute => 'test-time' };

ok $resp->is_success;
ok ($resp->content eq "");

$resp = CMD 'DUMP';

ok $resp->is_success;
my %p = parse_response 'DUMP', $resp->content;

ok (@{$p{Nodes}} == 1);
my $node = shift @{$p{Nodes}};
ok ($node->{JVMRoute} eq 'test-time');

ok($node->{flushwait} == 10);
ok($node->{ttl} == 60);
ok($node->{ping} == 10);
ok($node->{timeout} == 0);

# Clean after yourself
remove_nodes 'test-time';

##################################
### Check custom valid values  ###
##################################
$resp = CMD 'CONFIG', { JVMRoute => 'test-time', flushwait => 25, ttl => 30, ping => 65, timeout => 10 };

ok $resp->is_success;
ok ($resp->content eq "");

$resp = CMD 'DUMP';

ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;

$node = shift @{$p{Nodes}};
ok ($node->{JVMRoute} eq 'test-time');

ok($node->{flushwait} == 25);
ok($node->{ttl} == 30);
ok($node->{ping} == 65);
ok($node->{timeout} == 10);

# Clean after yourself
remove_nodes 'test-time';
sleep 25; # to have enough time for the removal

#################################
### Check some invalid values ###
#################################
$resp = CMD 'CONFIG', { JVMRoute => 'test-time', flushwait => -30 };
ok $resp->is_error;

$resp = CMD 'DUMP';
ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;
ok (@{$p{Nodes}} == 0);

$resp = CMD 'CONFIG', { JVMRoute => 'test-time', ttl => -1000 };
ok $resp->is_error;

$resp = CMD 'DUMP';
ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;
ok (@{$p{Nodes}} == 0);

$resp = CMD 'CONFIG', { JVMRoute => 'test-time', ping => -300 };
ok $resp->is_error;

$resp = CMD 'DUMP';
ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;
ok (@{$p{Nodes}} == 0);

$resp = CMD 'CONFIG', { JVMRoute => 'test-time', timeout => -10 };
ok $resp->is_error;

$resp = CMD 'DUMP';
ok $resp->is_success;
%p = parse_response 'DUMP', $resp->content;
ok (@{$p{Nodes}} == 0);


END {
    # Clean after yourself even with failure
    remove_nodes 'test-time';
    sleep 25;
}

