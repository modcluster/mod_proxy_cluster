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

plan tests => 25, need_mpc;

# first, install two nodes, we'll use them later
my $resp = CMD 'CONFIG', { JVMRoute => "route" };
ok t_cmp($resp->is_success, 1, "Adding route node");

$resp = CMD 'CONFIG', { JVMRoute => "route2", Type => "http", Port => $appport, Host => $apphost };
ok t_cmp($resp->is_success, 1, "Adding route2 node");

# first wait for sync
sleep 2;

# the default without any params
$resp = CMD 'PING';
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=PING-RSP&State=OK&id=\d+$/);

# with JVMRoute
$resp = CMD 'PING', { JVMRoute => "route" };
ok $resp->is_success;
# The node `route` does not exist, so it will be NOTOK
ok t_cmp($resp->content, qr/^Type=PING-RSP&JVMRoute=route&State=NOTOK&id=\d+$/, "PING response for 'route'");

$resp = CMD 'PING', { JVMRoute => "route2" };
ok $resp->is_success;
# The node `route2` corresponds to the fake app, so it should be OK
ok t_cmp($resp->content, qr/^Type=PING-RSP&JVMRoute=route2&State=OK&id=\d+$/, "PING response for 'route2'");

# with Host + Port + Scheme
$resp = CMD 'PING', { Scheme => "http", Port => $appport, Host => $apphost };
ok t_cmp($resp->is_success, 1);
# Same as previously
ok t_cmp($resp->content, qr/^Type=PING-RSP&State=OK&id=\d+$/, "PING response for 'http://$apphost:$appport'");

# with Host + Port + Scheme, but now with upgrade (On by default)
$resp = CMD 'PING', { Scheme => "ws", Port => $appport, Host => $apphost };
ok t_cmp($resp->is_success, 1);
# Same as previously
ok t_cmp($resp->content, qr/^Type=PING-RSP&State=OK&id=\d+$/, "PING response for 'ws://$apphost:$appport'");

# only JVMRoute is used
$resp = CMD 'PING', { JVMRoute => "route", Scheme => "http", Port => $appport, Host => $apphost };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=PING-RSP&JVMRoute=route&State=NOTOK&id=\d+$/, "PING response with all arguments behaves as with JVMRoute only");


# incorrect commands -> nonexisting JVMRoute
$resp = CMD 'PING', { JVMroute => "nonexsiting-route" };
ok $resp->is_error;
# TODO: Check the error message (cannot read the given node)

# incorrect commands -> something is missing (won't get an error but State=NOTOK)
$resp = CMD 'PING', { Scheme => "http", Port => $appport };
ok $resp->is_error;
# ok t_cmp($resp->content, qr/^Type=PING-RSP&State=NOTOK&id=\d+$/);

$resp = CMD 'PING', { Scheme => "http", Host => $apphost };
ok $resp->is_error;
# ok t_cmp($resp->content, qr/^Type=PING-RSP&State=NOTOK&id=\d+$/);

$resp = CMD 'PING', { Host => $apphost, Port => $appport };
ok $resp->is_error; 
# ok t_cmp($resp->content, qr/^Type=PING-RSP&State=NOTOK&id=\d+$/);

# Scheme is not supported
$resp = CMD 'PING', { Scheme => "udp", Host => $apphost, Port => $appport };
ok $resp->is_error;

# incorrect commands -> nonexisting hots/ports/schemes
# Scheme does not exist; TODO: Is this the right approach?
$resp = CMD 'PING', { Scheme => "ajp", Host => $apphost, Port => $appport };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=PING-RSP&State=NOTOK&id=\d+$/, "Scheme does not match the node");

# Host does not exist; TODO: Is this the right approach?
$resp = CMD 'PING', { Scheme => "http", Host => "nonexisting.host", Port => $appport };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=PING-RSP&State=NOTOK&id=\d+$/, "Host does not match the node");

# Port does not exist; TODO: Is this the right approach?
$resp = CMD 'PING', { Scheme => "http", Host => $apphost, Port => 1234 };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=PING-RSP&State=NOTOK&id=\d+$/, "Port does not match the node");


# Clean after yourself by a simple restart of the server
END {
    remove_nodes "route", "route2";
    sleep 25;
}

