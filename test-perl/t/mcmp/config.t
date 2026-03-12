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


plan tests => 310, need_mpc;


# CONFIG wihtout JVMRoute should fail
my $resp = CMD 'CONFIG';
ok $resp->is_error;
# TODO: Check the error message

$resp = CMD 'INFO';
ok $resp->is_success;
my %p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Nodes}}, 0, "There are no nodes after failed CONFIG (1)");

# CONFIG with Alias without Context should fail and vice-versa
$resp = CMD 'CONFIG', { JVMRoute => "route", Context => "/mycontext" };
ok $resp->is_error;
# TODO: Check the error message
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Nodes}}, 0, "There are no nodes after failed CONFIG (2)");

$resp = CMD 'CONFIG', { JVMRoute => "route", Alias => "myalias" };
ok $resp->is_error;
# TODO: Check the error message
$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;
ok t_cmp(@{$p{Nodes}}, 0, "There are no nodes after failed CONFIG (3)");


# Add CONFIG with route only
$resp = CMD 'CONFIG', { JVMRoute => "route" };
ok $resp->is_success;

$resp = CMD 'INFO';
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 1, "There is a single node");
ok t_cmp(@{$p{Contexts}}, 0, "There are no contexts");
ok t_cmp(@{$p{Hosts}}, 0, "There are no Aliases");


# Now we'll add a node with an app and check what a repeated CONFIG does in case of changed:
#     - i) node config parameters: only those parameters that can change, app remains working
#    - ii) other parameteres: the current node conflicts with the CONFIG, so the CONFIG is denied
#
#    TODO: What about following? - ii) other parameteres: the current node with the app gets removed and a new one gets added
#
# + if there's a combination of i) and ii), it behaves as ii).
# + if the JVMRoute differs but ii) overlaps with an existing node, that's a BAD request (and should be denied)
my @node_config  = ( { opt => "Flushpackets",        value => "on"          }
                   , { opt => "Flushwait",           value => "30000"       }
                   , { opt => "Ping",                value => "15"          }
                   , { opt => "Timeout",             value => "10"          }
                   , { opt => "StickySession",       value => "no"          }
                   , { opt => "StickySessionCookie", value => "MYCOOKIE"    }
                   , { opt => "StickySessionPath",   value => "mycookie"    }
                   , { opt => "StickySessionRemove", value => "yes"         }
                   , { opt => "StickySessionForce",  value => "no"          }
                   , { opt => "WaitWorker",          value => "5"           }
                   , { opt => "MaxAttempts",         value => "3"           }
                   , { opt => "Domain",              value => "example.com" }
                   );

# we'll skip JVMRoute here because that's the special case
# as well as Context + Alias that must go together
my @other_config = ( { opt => "Balancer", value => "myotherbalancer" }
                   , { opt => "Host",     value => "myhost"          }
                   , { opt => "Port",     value => "8888"            }
                   , { opt => "Type",     value => "ajp"             }
                   , { opt => "Reversed", value => "yes"             }
                   , { opt => "Smax",     value => "5"               }
                   , { opt => "Ttl",      value => "72"              }
                   );

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport };
ok $resp->is_success;

$resp = GET "/news";
ok t_cmp($resp->is_error, 1, "The /news context is not available through proxy without ENABLE-APP");

$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "myalias" };
ok $resp->is_success;

$resp = GET "/news";
ok t_cmp($resp->is_success, 1, "The /news context became available through proxy after ENABLE-APP");

$resp = CMD 'INFO';
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 2, "There are two nodes now");
ok t_cmp(@{$p{Contexts}}, 1, "There is the /news context");
ok t_cmp(@{$p{Hosts}}, 1, "There is the myalias alias");

# i)
foreach my $param (@node_config) {
    $resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost,
                            Port => $appport, $param->{opt} => $param->{value} };
    ok $resp->is_success;
    $resp = GET "/news";
    ok t_cmp($resp->is_success, 1, "Successful response after CONFIG with " . %$param{opt});
}

# ii)
foreach my $param (@other_config) {
    # to prevent multiple records of the same parameter, let's do it this way
    my %default = ( JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport );
    # this will override the default if present
    $default{$param->{opt}} = $param->{value};

    $resp = CMD 'CONFIG', \%default;
    ok $resp->is_error;

    $resp = GET "/news";
    ok t_cmp($resp->is_success, 1, "The /news endpoint should be still accessible after config with $param->{opt}=$param->{value}");

    # TODO: Reconsider whether this does not make more sense (see the ii) alternative above)
    #     ok $resp->is_success;
    #     $resp = GET "/news";
    # ok t_cmp($resp->is_error, 1, "CONFIG with " . %$param{opt} . " should remove/add node, thus removing the app");
    #
    # The following is not tested as the new parameters would lead to an error (modified node is not reachable)
    # $resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app", Context => "/news", Alias => "myalias" };
    # ok $resp->is_success;
    # $resp = GET "/news";
    # ok $resp->is_success;
}


# TODO: Isn't this about APP commands here??? Try CONFIG with Context and ALIAS instead!

# Remove the app and add it with multiple entries for context and alias
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = GET "/news";
ok $resp->is_error;

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport
                      , Context => "/first,/news,/last", Alias => "news.example.com,myhost,example.com" };
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 2, "There are two nodes now");
ok t_cmp(@{$p{Hosts}}, 3, "There should be 3 aliases");
ok t_cmp(@{$p{Contexts}}, 9, "There should be 9 contexts (3 per each of the 3 aliases)");

# The following should fail, because all contexts are STOPPED by default
$resp = GET "/news";
ok t_cmp($resp->is_error, 1, "The app should NOT be available again for /news");
$resp = GET "/first";
ok t_cmp($resp->is_error, 1, "The app should NOT be available again for /first");
$resp = GET "/last";
ok t_cmp($resp->is_error, 1, "The app should NOT be available again for /last");

foreach my $context (@{$p{Contexts}}) {
    ok t_cmp($context->{Status}, 'STOPPED', "All contexts are enabled");
}

# Enable all the contexts
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = GET "/news";
ok t_cmp($resp->is_success, 1, "The app should be available again for /news");
$resp = GET "/first";
ok t_cmp($resp->is_success, 1, "The app should be available again for /first");
$resp = GET "/last";
ok t_cmp($resp->is_success, 1, "The app should be available again for /last");

# Now do the same, but we give the list as repeated entries for a given parameter
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = CMD_internal 'CONFIG', '/', "JVMRoute=fake-app&Type=http&Host=$apphost&Port=$appport&Context=/first&Context=/news&Alias=news.example.com&Alias=myhost&Alias=example.com&Context=/last";
ok $resp->is_success;

$resp = CMD 'INFO';
ok $resp->is_success;
%p = parse_response 'INFO', $resp->content;

ok t_cmp(@{$p{Nodes}}, 2, "There are two nodes now");
ok t_cmp(@{$p{Hosts}}, 3, "There should be 3 aliases");
ok t_cmp(@{$p{Contexts}}, 9, "There should be 9 contexts (3 per each of the 3 aliases)");

# The following should fail, because all contexts are STOPPED by default
$resp = GET "/news";
ok t_cmp($resp->is_error, 1, "The app should NOT be available again for /news");
$resp = GET "/first";
ok t_cmp($resp->is_error, 1, "The app should NOT be available again for /first");
$resp = GET "/last";
ok t_cmp($resp->is_error, 1, "The app should NOT be available again for /last");

foreach my $context (@{$p{Contexts}}) {
    ok t_cmp($context->{Status}, 'STOPPED', "All contexts are enabled");
}

# Enable all the contexts
$resp = CMD 'ENABLE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

$resp = GET "/news";
ok t_cmp($resp->is_success, 1, "The app should be available again for /news");
$resp = GET "/first";
ok t_cmp($resp->is_success, 1, "The app should be available again for /first");
$resp = GET "/last";
ok t_cmp($resp->is_success, 1, "The app should be available again for /last");

# Check that we get an error with CONFIG when number of contexts exceeds the limit
# BUT also that the successfully added contexts are working.
## First clean everything
$resp = CMD 'REMOVE-APP', { JVMRoute => "fake-app" }, "/*";
ok $resp->is_success;

my @contexts = map { "/context$_" } (0..100);

$resp = CMD 'CONFIG', { JVMRoute => "fake-app", Type => "http", Host => $apphost, Port => $appport
                      , Context => join(',', @contexts), Alias => "localhost" };
ok t_cmp($resp->is_error, 1, "CONFIG gives an error because the last Context couldn't be added");

# check that the failed context is reported and is not reachable
my $failed_context = pop @contexts;
ok t_cmp($resp->header('Mess'), qr/\($failed_context and alias:/, "Context $failed_context is reported as failed");

# before trying to reach it, we must ENABLE everything (the default state is STOPPED!)
my $rsp = CMD 'ENABLE-APP', { JVMRoute => "fake-app" }, "/*";
ok $rsp->is_success;

$rsp = CMD 'INFO';
ok $rsp->is_success;
%p = parse_response 'INFO', $rsp->content;
ok t_cmp(@{$p{Contexts}}, 100, "There should be 100 contexts");

$rsp = GET $failed_context;
ok t_cmp($rsp->is_error, 1, "$failed_context is not reachable: " . $rsp->code);

# check that the rest is there & reachable
foreach my $present_context (@contexts) {
    ok t_cmp($resp->header('Mess'), qr/(?!\($present_context and alias:)/, "Check that $present_context was not reported in CONFIG error message");
    $rsp = GET $present_context;
    ok t_cmp($rsp->is_success, 1, "$present_context is reachable: " . $rsp->code);
}


# TODO: Check the individual parameters ranges

# Clean after yourself by a simple restart of the server
END {
    my $ret = $?;
    my $cfg = Apache::Test::config();
    my $server = $cfg->server;
    $server->stop();
    $server->start();
    $? = $ret;
}

