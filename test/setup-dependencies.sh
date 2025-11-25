#!/usr/bin/sh

# create a docker network if it does not exist
docker network create mod_proxy_cluster_testsuite_net > /dev/null 2>&1 || true

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
mkdir -p dependencies/ && cd dependencies/

if [ -d $TEST_DIR/tomcat/target ]; then
    rm $TEST_DIR/tomcat/target/*
else
    mkdir $TEST_DIR/tomcat/target/
fi

# get websocket demo repository
if [ ! -d httpd_websocket-testsuite ]; then
    git clone https://github.com/modcluster/ci.modcluster.io ci.modcluster.io
    mv ci.modcluster.io/websocket-hello httpd_websocket-testsuite
    rm -rf ci.modcluster.io
fi
cd httpd_websocket-testsuite
git pull --rebase
mvn --batch-mode --no-transfer-progress install || exit 1
cp target/websocket-hello-0.0.1.war $TEST_DIR/websocket/
cd ..

# get mod_cluster (Java/Tomcat part)
if [ ! -d mod_cluster-testsuite ]; then
    git clone https://github.com/modcluster/mod_cluster mod_cluster-testsuite
fi
cd mod_cluster-testsuite
git pull --rebase
mvn --batch-mode --no-transfer-progress clean install || exit 2
cp dist/target/*.zip $TEST_DIR/tomcat/target/
cd ..

# prepare tomcat test apps
cd $TEST_DIR
mvn --batch-mode --no-transfer-progress -f includes/pom-groovy.xml clean install || exit 4
