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
