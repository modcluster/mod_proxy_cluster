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

plan tests => 22;

Apache::TestRequest::module("mpc_test_host");
my $hostport = Apache::TestRequest::hostport();

my $url = "http://$hostport/";
my $resp = GET $url;

ok $resp->is_success;
ok (index($resp->as_string, "mod_cluster/2.0.0.Alpha1-SNAPSHOT") != -1);


##################
##### CONFIG #####
##################

## Missing JVMRoute
$resp = CMD 'CONFIG', $url;

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: JVMRoute can't be empty");

## Empty JVMRoute
$resp = CMD 'CONFIG', $url, ( JVMRoute => '' );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
# ok ($resp->header("Mess") eq "SYNTAX: Can't parse MCMP message. It might have contained illegal symbols or unknown elements.");
ok ($resp->header("Mess") eq "SYNTAX: JVMRoute can't be empty");

# Too long Alias and Context

my $long_alias = "a" x 256;
my $long_context = "c" x 81;

$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Alias => $long_alias );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Alias field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Context => $long_context );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Context field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## TODO: Test other LIMITS

