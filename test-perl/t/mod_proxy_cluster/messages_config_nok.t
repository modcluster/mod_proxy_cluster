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

plan tests => 70;

Apache::TestRequest::module("mpc_test_host");
my $hostport = Apache::TestRequest::hostport();

my $url = "http://$hostport/";
my $resp = GET $url;

ok $resp->is_success;
ok (index($resp->as_string, "mod_cluster/2.0.0.Alpha1-SNAPSHOT") != -1);

my $is_httpd2_4 = $resp->header('Server') =~ m/Apache\/2\.4/;

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

# LIMITS
## Alias
my $long_alias = "a" x 256;

$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Alias => $long_alias );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Alias field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## Context
my $long_context = "c" x 81;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Context => $long_context );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Context field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## Balancer
my $long_balancer = "b" x 41;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Balancer => $long_balancer );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Balancer field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## JVMRoute
my $long_route = "r" x ($is_httpd2_4 ? 65 : 97);
$resp = CMD 'CONFIG', $url, ( JVMRoute => $long_route );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: JVMRoute field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node $long_route") == -1);

## Domain
my $long_domain = "d" x 21;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Domain => $long_domain );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: LBGroup field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## Host
my $long_host = "d" x 65;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Host => $long_host );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Host field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## Port
my $long_port = "p" x 8;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Port => $long_port );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Port field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## Type
my $long_type = "t" x 17;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', Type => $long_type );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: Type field too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## StickySessionCookie
my $long_cookie = "d" x 31;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', StickySessionCookie => $long_cookie );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: A field is too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);

## StickySessionPath
my $long_path = "p" x 31;
$resp = CMD 'CONFIG', $url, ( JVMRoute => 'spare', StickySessionPath => $long_path );

ok $resp->is_error;
ok ($resp->content ne "");
ok ($resp->header("Type") eq "SYNTAX");
ok ($resp->header("Mess") eq "SYNTAX: A field is too big");

$resp = GET "$url/mod_cluster_manager";
ok $resp->is_success;
ok (index($resp->as_string, "Node spare") == -1);
