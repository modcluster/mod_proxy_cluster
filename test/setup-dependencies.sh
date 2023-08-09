# Run this from the same directory
TEST_DIR=$(pwd)
cd ../..
# get websocket demo repository
git clone https://github.com/jfclere/httpd_websocket
cd httpd_websocket
mvn install || exit 1
cp target/websocket-hello-0.0.1.war $TEST_DIR
cd ..

# get mod_cluster (Java/Tomcat part)
git clone https://github.com/modcluster/mod_cluster
cd mod_cluster
mvn install || exit 2
cd $TEST_DIR

# prepare jars
mvn install

