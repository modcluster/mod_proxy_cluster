# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Apache-ModProxyCluster.t'
#########################

use strict;
use warnings;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestRequest 'GET';


plan tests => 3;

Apache::TestRequest::module("fake_cgi_app");
my $hostport = Apache::TestRequest::hostport();

my $url = "http://$hostport/news";
my $data = GET $url;

ok $data->is_success;

ok (index($data->as_string, "REDIRECT_") == -1);
ok (index($data->as_string, "Fake App!") != -1);
