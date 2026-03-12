# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestServer;
use Apache::TestRequest 'GET';

use ModProxyCluster;

# get the fake cgi app host and port
Apache::TestRequest::module("fake_cgi_app");
my ($apphost, $appport) = split ':', Apache::TestRequest::hostport();
Apache::TestRequest::module("mpc_test_host");

plan tests => 366, need_mpc;


foreach my $cmd ('ENABLE-APP', 'STOP-APP', 'DISABLE-APP', 'REMOVE-APP') {
    # missing context and alias
    my $resp = CMD $cmd, { JVMRoute => 'no-app' };
    ok $resp->is_error;
    # TODO: Check the error message
    
    # missing only context
    $resp = CMD $cmd, { JVMRoute => 'no-app', Context => "mycontext" };
    ok $resp->is_error;
    # TODO: Check the error message
    
    
    # missing only alias
    $resp = CMD $cmd, { JVMRoute => 'no-app', Alias => "myalias" };
    ok $resp->is_error;
    # TODO: Check the error message
    
    
    # try the command without an existing node
    $resp = CMD $cmd, { JVMRoute => 'no-app', Context => "mycontext", Alias => "myalias" };
    ok $resp->is_error;
    # TODO: Check the error message
}

# Add a node with the fake cgi app
my $resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport };
ok $resp->is_success;

# Check that the app is present
$resp = GET "http://$apphost:$appport/";
ok $resp->is_success;
ok t_cmp($resp->content, qr/Fake App!/, "Reach the app directly");

# Check that the context (pointing to the app) is not present
$resp = GET "/news";
ok $resp->is_error;

# Now enable the app
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;
ok t_cmp($resp->content, qr/ENABLE-APP-RSP/);

# Check that the app is present & accessible through the proxy
$resp = GET "/news";
ok $resp->is_success;
ok t_cmp($resp->content, qr/Fake App!/, "Reach the app through the proxy");

# Check the app is reported & its state is ENABLED
$resp = CMD 'INFO';
ok $resp->is_success;
my %p = parse_response 'INFO', $resp->content;

ok t_cmp($p{Contexts}->[0]{Context}, "/news");
ok t_cmp($p{Contexts}->[0]{Status}, "ENABLED");

# Now DISABLE the app & check it's unreachable but present
$resp = CMD 'DISABLE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;
ok t_cmp($resp->content, qr/DISABLE-APP-RSP/);

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp($p{Contexts}->[0]{Context}, "/news");
ok t_cmp($p{Contexts}->[0]{Status}, "DISABLED");

# Now STOP, so basically the same, but the Status is different
$resp = CMD 'STOP-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;
ok t_cmp($resp->content, qr/STOP-APP-RSP/);

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp($p{Contexts}->[0]{Context}, "/news");
ok t_cmp($p{Contexts}->[0]{Status}, "STOPPED");

# Let's ENABLE it again
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;

# Check that the app is present
$resp = GET "/news";
ok $resp->is_success;
ok t_cmp($resp->content, qr/Fake App!/, "The app is reachable again");

# Check the app is reported & its state is ENABLED
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp($p{Contexts}->[0]{Context}, "/news");
ok t_cmp($p{Contexts}->[0]{Status}, "ENABLED");

# Now we REMOVE the app
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;
ok t_cmp($resp->content, qr/REMOVE-APP-RSP/);

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Contexts}}, 0, "There are no longer any contexts");
ok t_cmp(@{$p{Hosts}}, 0, "There are no longer any aliases");
# but the node is still there
ok t_cmp(@{$p{Nodes}}, 1, "The node is still present but with no Contexts or Aliases");

# Now check that DISABLE and STOP add the app back (with the right status) but REMOVE doesn't
# 1) DISABLE
$resp = CMD 'DISABLE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp($p{Contexts}->[0]{Context}, "/news");
ok t_cmp($p{Contexts}->[0]{Status}, "DISABLED");

$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Contexts}}, 0, "There are no longer any contexts");
ok t_cmp(@{$p{Hosts}}, 0, "There are no longer any aliases");
# but the node is still there
ok t_cmp(@{$p{Nodes}}, 1, "The node is still present but with no Contexts or Aliases");

# 2) STOP
$resp = CMD 'STOP-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp($p{Contexts}->[0]{Context}, "/news");
ok t_cmp($p{Contexts}->[0]{Status}, "STOPPED");

$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;

# 3) REMOVE
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "localhost" };
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Contexts}}, 0, "There are no longer any contexts");
ok t_cmp(@{$p{Hosts}}, 0, "There are no aliases");

# ######################################### #
# Now let's test wildcarded/global commands #
# ######################################### #
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/one,/two", Alias => "localhost,example.com" };
ok t_cmp($resp->is_success, 1, "ENABLE-APP with 2 contexts in 2 aliases should be ok");

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "There's a single node");
ok t_cmp(@{$p{Hosts}}, 2, "There are two aliases");
ok t_cmp(@{$p{Contexts}}, 4, "There are four contexts (2 per alias)");

foreach my $context (@{$p{Contexts}}) {
    ok t_cmp($context->{Status}, 'ENABLED', "All contexts are enabled");
}

$resp = CMD 'DISABLE-APP', { JVMRoute => "fake-app" }, "/*";
ok t_cmp($resp->is_success, 1, "DISABLE-APP with 2 contexts in 2 aliases should be ok");

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Contexts}}, 4, "There are still four contexts (2 per alias)");

foreach my $context (@{$p{Contexts}}) {
    ok t_cmp($context->{Status}, 'DISABLED', "All contexts are disabled");
}

$resp = CMD 'STOP-APP', { JVMRoute => "fake-app" }, "/*";
ok t_cmp($resp->is_success, 1, "STOP-APP with 2 contexts in 2 aliases should be ok");

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Contexts}}, 4, "There are still four contexts (2 per alias)");

foreach my $context (@{$p{Contexts}}) {
    ok t_cmp($context->{Status}, 'STOPPED', "All contexts are stopped");
}

$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app" }, "/*";
ok t_cmp($resp->is_success, 1, "ENABLE-APP should bring us back to where we started");

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Contexts}}, 4, "There are four contexts (2 per alias)");
foreach my $context (@{$p{Contexts}}) {
    ok t_cmp($context->{Status}, 'ENABLED', "All contexts are enabled again");
}

$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok t_cmp($resp->is_success, 1, "REMOVE-APP should clean everything now with the wildcard");

sleep 11; # necessary to see the removal

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Contexts}}, 0, "There are no longer any contexts");
ok t_cmp(@{$p{Hosts}}, 0, "There are no aliases");
ok t_cmp(@{$p{Nodes}}[0]->{Name}, 'REMOVED', "Even the node was removed");

# ############################################################### #
# Now let's try bunch of aliases and contexts in a single command #
# ############################################################### #
$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport };
ok $resp->is_success;

# Make sure the node gets routed, no 503
CMD 'STATUS', { JVMRoute => "fake-app", Load => 100 };

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "There is a single node");
ok t_cmp(@{$p{Hosts}}, 0, "The alias got removed");
ok t_cmp(@{$p{Contexts}}, 0, "The context got removed");

$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/first,/news,/last", Alias => "news.example.com,myhost,example.com" };
ok t_cmp($resp->is_success, 1, "ENABLE-APP with 3 contexts and 3 aliases should be ok");

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "There is a single node");
ok t_cmp(@{$p{Hosts}}, 3, "There should be 3 aliases");
ok t_cmp(@{$p{Contexts}}, 9, "There should be 9 contexts (3 per each of the 3 aliases)");

$resp = GET "/news";
ok t_cmp($resp->is_success, 1, "The app should be available again for /news");
$resp = GET "/first";
ok t_cmp($resp->is_success, 1, "The app should be available again for /first");
$resp = GET "/last";
ok t_cmp($resp->is_success, 1, "The app should be available again for /last");

# Now do the same, but we give the list as repeated entries for a given parameter
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport };
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'INFO';
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "The node remains present");
ok t_cmp(@{$p{Hosts}}, 0, "The alias got removed");
ok t_cmp(@{$p{Contexts}}, 0, "The context got removed");

$resp = CMD_internal 'ENABLE-APP', '/', 'JVMRoute=fake-app&Context=/first&Context=/news&Alias=news.example.com&Alias=myhost&Alias=example.com&Context=/last';
ok t_cmp($resp->is_success, 1, "ENABLE-APP with 3 contexts and aliases as repeated parameters should be ok too");

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "The node remains present");
ok t_cmp(@{$p{Hosts}}, 3, "There should be 3 aliases");
ok t_cmp(@{$p{Contexts}}, 9, "There should be 9 contexts (3 per each of the 3 aliases)");

$resp = GET "/news";
ok t_cmp($resp->is_success, 1, "The app should be available again for /news");
$resp = GET "/first";
ok t_cmp($resp->is_success, 1, "The app should be available again for /first");
$resp = GET "/last";
ok t_cmp($resp->is_success, 1, "The app should be available again for /last");

# Now do the same again, but this time we'll send one command per each context+alias
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport };
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

foreach my $alias ("news.example.com", "myhost", "example.com") {
    foreach my $context ("/first", "/news", "/last") {
        $resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => $context, Alias => $alias };
        ok t_cmp($resp->is_success, 1, "ENABLE-APP with context $context and alias $alias should be ok");

        $resp = GET $context;
        ok t_cmp($resp->is_success, 1, "The app should be available again for $context");
    }
}

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "There is the node still present");
ok t_cmp(@{$p{Hosts}}, 3, "There should be 3 aliases");
ok t_cmp(@{$p{Contexts}}, 9, "There should be 9 contexts (3 per each of the 3 aliases)");


# We now add the node again and add more contexts that is the capacity. We check that the command
# does not fail, it adds all but the last context and these are reachable.
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport };
ok $resp->is_success;

my @contexts = map { "/context$_" } (0..100);

$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => join(',', @contexts), Alias => "localhost" };
ok t_cmp($resp->is_success, 1, "ENABLE-APP ok for context a set of contexts");

# check that the missing context is missing & not reachable
my $missing_context = pop @contexts;
ok t_cmp($resp->content, qr/(?!$missing_context)/, "Context $missing_context is missing");
my $rsp = GET $missing_context;
ok t_cmp($rsp->is_error, 1, "$missing_context is missing and thus not reachable: " . $rsp->code);

# check that the rest is there & reachable
foreach my $present_context (@contexts) {
    ok t_cmp($resp->content, qr/$present_context(,|\n)/, "Checking $present_context is present");
    $rsp = GET $present_context;
    ok t_cmp($rsp->is_success, 1, "$present_context is reachable: " . $rsp->code);
}

# Clean after yourself by a simple restart of the server
END {
    my $ret = $?;
    my $cfg = Apache::Test::config();
    my $server = $cfg->server;
    $server->stop();
    $server->start();
    $? = $ret;
}

