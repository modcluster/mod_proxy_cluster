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

plan tests => 8;

Apache::TestRequest::module("mpc_test_host");
my $hostport = Apache::TestRequest::hostport();

my $url = "http://$hostport/";
my $resp = GET $url;

ok $resp->is_success;
ok (index($resp->as_string, "mod_cluster/2.0.0.Alpha1-SNAPSHOT") != -1);

##################
##### STATUS #####
##################

$resp = CMD 'STATUS', $url;
ok $resp->is_error;

ok ($resp->header('Type') eq 'SYNTAX');
ok ($resp->header('Mess') eq 'SYNTAX: Invalid field "" in message');

my $nonexistent = "nonexistent";
$resp = CMD 'STATUS', $url, ( JVMRoute => $nonexistent);
ok $resp->is_error;

ok ($resp->header('Type') eq 'MEM');
ok ($resp->header('Mess') eq "MEM: Can't read node with \"$nonexistent\" JVMRoute");
