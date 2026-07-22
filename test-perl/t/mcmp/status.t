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

plan tests => 30, need_mpc;


my $resp = CMD 'CONFIG', { JVMRoute => "host" };
ok $resp->is_success;

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Port => $appport, Host => $apphost };
ok $resp->is_success;

# Status with no node specified
$resp = CMD 'STATUS';
ok t_cmp($resp->is_error, 1, "STATUS should fail when missing JVMRoute parameter");

# Status with non-existing node
$resp = CMD 'STATUS', { JVMRoute => "nonexsiting" };
ok t_cmp($resp->is_error, 1, "STATUS should fail for nonexisting node");

# Ask about the STATUS of the two nodes
$resp = CMD 'STATUS', { JVMRoute => "host" };
ok $resp->is_success;
# Might be NOTOK because we created the host by CONFIG without anything being there
ok t_cmp($resp->content, qr/^Type=STATUS-RSP&JVMRoute=host&State=(NOTOK|OK)&id=\d+$/, "Check for STATUS-RSP content (host)");

$resp = CMD 'STATUS', { JVMRoute => "fake-app" };
ok $resp->is_success;
# This is going to be ok, since there is the fake app behing it
ok t_cmp($resp->content, qr/^Type=STATUS-RSP&JVMRoute=fake-app&State=OK&id=\d+$/, "Check for STATUS-RSP content (fake-app)");

# Now with Load set
$resp = CMD 'STATUS', { JVMRoute => "fake-app", Load => 0 };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=STATUS-RSP&JVMRoute=fake-app&State=OK&id=\d+$/, "After Load=0");
# Check it was set
$resp = CMD 'INFO';
ok $resp->is_success;
my %p = parse_response 'INFO', $resp->content;
# TODO: Nodes should be probably addressable by their route
ok t_cmp($p{Nodes}->[0]{Load}, 0, "Load should be 0 for node $p{Nodes}->[0]{Name}");

$resp = CMD 'STATUS', { JVMRoute => "fake-app", Load => 100 };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=STATUS-RSP&JVMRoute=fake-app&State=OK&id=\d+$/, "After Load=100");
# Check it was set
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
# TODO: Nodes should be probably addressable by their route
ok t_cmp($p{Nodes}->[0]{Load}, 100, "Load should be 100 for node $p{Nodes}->[0]{Name}");

$resp = CMD 'STATUS', { JVMRoute => "fake-app", Load => -1 };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=STATUS-RSP&JVMRoute=fake-app&State=OK&id=\d+$/, "After Load=-1");
# Check it was set
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
# TODO: Nodes should be probably addressable by their route
ok t_cmp($p{Nodes}->[0]{Load}, -1, "Load should be -1 for node $p{Nodes}->[0]{Name}");

$resp = CMD 'STATUS', { JVMRoute => "fake-app", Load => 50 };
ok $resp->is_success;
ok t_cmp($resp->content, qr/^Type=STATUS-RSP&JVMRoute=fake-app&State=OK&id=\d+$/, "After Load=50");
# Check it was set
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
# TODO: Nodes should be probably addressable by their route
ok t_cmp($p{Nodes}->[0]{Load}, 50, "Load should be 50 for node $p{Nodes}->[0]{Name}");

# What about Load outside of the allowed range?
$resp = CMD 'STATUS', { JVMRoute => "fake-app", Load => 101 };
ok $resp->is_error;
# Check it was set
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
# TODO: Nodes should be probably addressable by their route
ok t_cmp($p{Nodes}->[0]{Load}, 50, "Load remains, Load=101 was ignored for $p{Nodes}->[0]{Name}");

$resp = CMD 'STATUS', { JVMRoute => "fake-app", Load => -2 };
ok $resp->is_error;
# Check it was set
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
# TODO: Nodes should be probably addressable by their route
ok t_cmp($p{Nodes}->[0]{Load}, 50, "Load remains, Load=-2 was ignored for node $p{Nodes}->[0]{Name}");


# Clean after yourself by a simple restart of the server
END {
    my $ret = $?;
    my $cfg = Apache::Test::config();
    my $server = $cfg->server;
    $server->stop();
    $server->start();
    $? = $ret;
}

