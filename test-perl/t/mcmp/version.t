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
Apache::TestRequest::module("mpc_test_host");

plan tests => 7, need_mpc;

my $resp = CMD 'VERSION';
ok $resp->is_success;

my $sw_info_pattern = '(mod_(?:proxy_)?cluster)\/(\d+\.\d+\.\d+\.(Dev|Alpha\d+|Beta\d+|CR\d+|Final))';

my ($sw_name, $sw_version, $proto_version) = $resp->content =~ /release: $sw_info_pattern, protocol: (\d+\.\d+\.\d+)/;

ok (defined $sw_name);
ok (defined $sw_version);
ok (defined $proto_version);

$resp = GET "/mod_cluster_manager";
# resp should contain sw name and version too, check it for consistency: mod_cluster/2.0.0.Alpha1-SNAPSHOT
my ($manager_sw_name, $manager_sw_version) = $resp->content =~ /$sw_info_pattern/;

ok t_cmp($sw_name,    $manager_sw_name);
ok t_cmp($sw_version, $manager_sw_version);

my ($reported_major, $reported_minor, $reported_patch, $reported_string) = mpc_version();
ok t_cmp("$reported_major.$reported_minor.$reported_patch.$reported_string", $sw_version);

