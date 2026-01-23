# Testsuite based on httpd-tests

This testsuite is based on perl testing framework for Apache.

To run these tests, you will need `httpd`, `perl` and `cpan`. You will be required to compile
this project as well, for more information see the `native/` directory at root of this
repository.

## Dependencies

On Fedora, install `perl-Test` and `perl-ExtUtils-MakeMaker`. Then install required packages
from `cpan`:

```
cpan install Bundle::ApacheTest Apache::TestMM HTTP::Request
```

Then compile the four modules provided by this repository and place the built modules into `t/modules/`
directory.

## Running tests

To run tests just execute following:

```
perl Makefile.PL
make
t/TEST
```

In case you have a custom installation of httpd, you'll be probably required to provide its path. E.g.,
in case of httpd installed under `/opt/apache2/`, execute following:

```
perl Makefile.PL -apxs /opt/apache2/bin/apxs
make
t/TEST -apxs /opt/apache2/bin/apxs
```

For more information about the testing framework see the official
[documentation](https://perl.apache.org/docs/general/testing/testing.html).

Alternatively, you can use a prepared Containerfile from
[ci.modcluster.io repository](https://github.com/modcluster/ci.modcluster.io/blob/main/misc/Containerfile.perltests).

