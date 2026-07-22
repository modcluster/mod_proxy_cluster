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
Apache::TestRequest::module("mpc_test_host");

plan tests => 7, need_mpc;

my $resp = GET "/";
ok $resp->is_success;

##################
##### STATUS #####
##################

$resp = CMD 'STATUS';
ok $resp->is_error;

ok ($resp->header('Type') eq 'SYNTAX');
ok ($resp->header('Mess') eq 'SYNTAX: Invalid field "" in message');

my $nonexistent = "nonexistent";
$resp = CMD 'STATUS', { JVMRoute => $nonexistent };
ok $resp->is_error;

ok ($resp->header('Type') eq 'MEM');
ok ($resp->header('Mess') eq "MEM: Can't read node with \"$nonexistent\" JVMRoute");
