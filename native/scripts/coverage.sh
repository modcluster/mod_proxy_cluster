#!/usr/bin/sh
# Collects code coverage of the modules built with -DENABLE_COVERAGE=ON.
#
#   coverage.sh capture <name> <output-dir>   gather the counters dumped so far
#   coverage.sh report <output-dir>           merge the gathered tracefiles into reports
#
# "capture" produces the coverage-<name>.json and coverage-<name>.info tracefiles,
# "report" merges every tracefile within the output directory into test-coverage.txt,
# test-coverage.html and lcov/index.html.
#
# The httpd instance under test has to be stopped before capturing, otherwise gcov
# has not dumped its counters yet and there is nothing to collect.
#
# The location of the sources is derived from the location of this script, override
# it with the NATIVE variable if needed.
#
# exits with 0 if the data were collected
# exits with 1 if there is nothing to collect
# exits with 2 when used incorrectly
# exits with 3 if one of the coverage tools failed

NATIVE=${NATIVE:-$(cd -- "$(dirname -- "$0")/.." && pwd)}

# gcovr is always given --root so that the paths within the tracefiles stay relative
# to native/. That keeps tracefiles captured from different runs (and even on
# different machines) mergeable. Mind that gcovr reads its own tracefiles only when
# they were written by the very same version, so all the jobs have to agree on one.
GCOVR="gcovr --gcov-ignore-parse-errors=negative_hits.warn_once_per_file --root $NATIVE"

# Prints the given message and gives up
fail() {
    echo "$1"
    exit 3
}

capture() {
    name=$1
    out=$2

    if [ -z "$(find $NATIVE -name '*.gcda' 2> /dev/null)" ]; then
        echo "No coverage data found in $NATIVE, is httpd stopped?"
        return 1
    fi

    mkdir -p $out
    $GCOVR --json $out/coverage-$name.json > $out/coverage-$name.log 2>&1 \
        || fail "gcovr failed to capture $name, see $out/coverage-$name.log"
    # lcov is confined to our sources the same way gcovr is by --root, otherwise the
    # httpd headers end up in the report as well (wherever they happen to live)
    lcov --capture --directory $NATIVE/build --ignore-errors gcov,negative \
         --include "$NATIVE/*" --output-file $out/coverage-$name.info \
         > $out/coverage-lcov-$name.log 2>&1 \
        || fail "lcov failed to capture $name, see $out/coverage-lcov-$name.log"
}

report() {
    out=$1

    if [ -z "$(ls $out/coverage-*.json 2> /dev/null)" ]; then
        echo "No tracefiles to report on within $out."
        return 1
    fi

    mkdir -p $out/lcov
    # the glob is quoted on purpose, it is gcovr who expands it
    $GCOVR --add-tracefile "$out/coverage-*.json" \
           --txt $out/test-coverage.txt --html-details $out/test-coverage.html \
           > $out/test-coverage.log 2>&1 \
        || fail "gcovr failed to merge the tracefiles, see $out/test-coverage.log"
    # unlike gcovr, lcov records absolute paths, so they are pointed back at these
    # sources; that is a no-op for the tracefiles captured here and it is what makes
    # the ones captured elsewhere merge (and render) instead of piling up side by side
    sed -i "s|^SF:.*/native/|SF:$NATIVE/|" $out/coverage-*.info
    genhtml --ignore-errors negative,empty $out/coverage-*.info \
            --output-directory $out/lcov > $out/lcov/test-coverage-lcov.log 2>&1 \
        || fail "genhtml failed, see $out/lcov/test-coverage-lcov.log"
}

usage() {
    echo "usage: $0 capture <name> <output-dir>"
    echo "       $0 report <output-dir>"
    exit 2
}

case "$1" in
capture)
    if [ -z "$2" ] || [ -z "$3" ]; then usage; fi
    capture "$2" "$3";;
report)
    if [ -z "$2" ]; then usage; fi
    report "$2";;
*)
    usage;;
esac
