use v5.32;
use ExtUtils::MakeMaker;
use Apache::TestMM qw(test clean);
use Apache::TestMM qw(test clean);
use Apache::TestReport ();
use Apache::TestSmoke ();
use Apache::TestRun ();

use File::Find qw(finddepth);

my @scripts = ();

finddepth(sub {
    return unless /^(?!.#)(.*?\.pl)\.PL$/;
    push @scripts, "$File::Find::dir/$1";
}, '.');

Apache::TestMM::filter_args();

for my $script (@scripts) {
	Apache::TestMM::generate_script($script);
}


WriteMakefile(
    NAME    => 'mpc-test',
    VERSION => '0.0.1'
);

