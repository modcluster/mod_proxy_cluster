#!/usr/bin/sh

# $1 is the command, $2 is the flag used for the check (--version by default)
check_cmd() {
if [ -z "$1" ]; then
        echo "check_cmd called without arguments"
        exit 11
    fi

    $1 ${2:-"--version"} > /dev/null 2>&1
    if [ $? != 0 ]; then
        echo "$1 is not available, please install $1"
        exit 3
    fi
}

# check that necessary tools and commands are available
check_cmd ab "-V"
for cmd in ss curl mvn docker jdb git
do
    check_cmd $cmd
done

# Run this from the same directory
TEST_DIR=$(pwd)
cd ../..

if [ -d $TEST_DIR/tomcat/target ]; then
    rm $TEST_DIR/tomcat/target/*
fi

# get websocket demo repository
if [ ! -d httpd_websocket-testsuite ]; then
    git clone https://github.com/jfclere/httpd_websocket httpd_websocket-testsuite
fi
cd httpd_websocket-testsuite
git pull --rebase
mvn install || exit 1
cp target/websocket-hello-0.0.1.war $TEST_DIR/websocket/
cd ..

# get mod_cluster (Java/Tomcat part)
if [ ! -d mod_cluster-testsuite ]; then
    git clone https://github.com/modcluster/mod_cluster mod_cluster-testsuite
fi
cd mod_cluster-testsuite
git pull --rebase
mvn install || exit 2
cd $TEST_DIR

# prepare jars
mvn install

# prepare tomcat test apps
mvn -f includes/pom-groovy.xml install
