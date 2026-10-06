#!/usr/bin/sh
# Entrypoint of the perl testsuite image.
# It uses the contaniner image from test/ testsuite.
# Runs it and gathers the code coverage when the underlying container has ENABLE_COVERAGE set to 1
#
# The test log ends up in t/logs/test-perl.log and the coverage data in /coverage.
#
# exits with 0 if every test passed

HTTPD=/usr/local/apache2/bin/httpd

# Apache::Test puts the ServerRoot at t/, so every module the generated config loads
# has to be found in t/modules -- the httpd ones included. Ours are symlinked among
# them by the image, cp resolves that for us.
mkdir -p t/modules
cp /usr/local/apache2/modules/*.so t/modules/ || exit 1

perl Makefile.PL -httpd $HTTPD || exit 1
make || exit 1

t/TEST -httpd $HTTPD 2>&1 | tee test-perl.log
mv test-perl.log t/logs/test-perl.log
grep -q "Result: PASS" t/logs/test-perl.log
res=$?

# t/TEST stops httpd on its own, so the counters are already dumped by now
if [ "$ENABLE_COVERAGE" = "1" ]; then
    /native/scripts/coverage.sh capture perl-tests /coverage \
        && /native/scripts/coverage.sh report /coverage
fi

exit $res
