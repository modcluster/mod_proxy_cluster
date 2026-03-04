# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;

use HTTP::Request;
use LWP::UserAgent;

use ModProxyCluster;
Apache::TestRequest::module("mpc_test_host");

plan tests => 27, need_mpc;

my $hostport = Apache::TestRequest::hostport();

my $resp = CMD 'CONFIG', { JVMRoute => "issue-329", Context => "/news,/test", Alias => "testalias,example" };
ok $resp->is_success;


$resp = CMD 'INFO';
ok $resp->is_success;
my %p = parse_response 'INFO', $resp->content;

ok(@{$p{Nodes}} == 1);

ok (@{$p{Contexts}} == 2);
ok ($p{Contexts}->[0]{Context} eq '/news');
ok ($p{Contexts}->[1]{Context} eq '/test');

ok (@{$p{Hosts}} == 2);
ok ($p{Hosts}->[0]{Alias} eq 'testalias');
ok ($p{Hosts}->[1]{Alias} eq 'example');

# Clean after yourself
remove_nodes 'issue-329';
sleep 25; # just to make sure we'll have enough time to get it removed

#################################################
### THIS SHOULD BE EQUIVALENT TO THE PREVIOUS ###
#################################################

$resp = CMD_internal 'CONFIG', '/', "JVMRoute=issue-329&Alias=testalias&Context=/news,/test&Alias=example";
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok(@{$p{Nodes}} == 1);

ok (@{$p{Contexts}} == 2);
ok ($p{Contexts}->[0]{Context} eq '/news');
ok ($p{Contexts}->[1]{Context} eq '/test');

ok (@{$p{Hosts}} == 2);
ok ($p{Hosts}->[0]{Alias} eq 'testalias');
ok ($p{Hosts}->[1]{Alias} eq 'example');

# Clean after yourself
remove_nodes 'issue-329';
sleep 25; # just to make sure we'll have enough time to get it removed

#################################################
### THIS SHOULD BE EQUIVALENT AS THE ALIAS IS ###
#################################################
$resp = CMD_internal 'CONFIG', '/', "JVMRoute=issue-329&Alias=testalias&Context=/news&Alias=example&Context=/test";
ok $resp->is_success;


$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok(@{$p{Nodes}} == 1);

ok (@{$p{Contexts}} == 2);
ok ($p{Contexts}->[0]{Context} eq '/news');
ok ($p{Contexts}->[1]{Context} eq '/test');

ok (@{$p{Hosts}} == 2);
ok ($p{Hosts}->[0]{Alias} eq 'testalias');
ok ($p{Hosts}->[1]{Alias} eq 'example');

END {
    # Clean after yourself
    remove_nodes 'issue-329';
    sleep 25; # just to make sure we'll have enough time to get it removed
}

