use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestConfig;
use Apache::TestRequest 'GET';

plan tests => 1;

my $res = GET '/';

ok (index($res->code, 200) != -1);
