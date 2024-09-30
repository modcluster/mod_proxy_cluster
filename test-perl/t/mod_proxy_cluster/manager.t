# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestRequest 'GET_BODY';

# use Test::More;
use ModProxyCluster;


plan tests => 5;

ok 1; # simple load test

Apache::TestRequest::module("mpc_test_host");
my $hostport = Apache::TestRequest::hostport();

my $url = "http://$hostport/mod_cluster_manager";
my $data = GET_BODY $url;

ok (index($data, "mod_cluster/2.0.0.Alpha1-SNAPSHOT") != -1);
ok (index($data, "Node") == -1);

my %h = ( JVMRoute => 'next', Host => '127.0.0.2', Port => '8082', Type => 'http' );
$data = CMD 'CONFIG', "http://$hostport/", %h;

ok $data->is_success;

$data = GET_BODY $url;

ok (index($data, "Node next") != -1);
