# Run this from the same directory
TEST_DIR=$(pwd)
cd ../..
# get websocket demo repository
if [ ! -d httpd_websocket ]; then
    git clone https://github.com/jfclere/httpd_websocket
fi
cd httpd_websocket
git pull --rebase
mvn install || exit 1
cp target/websocket-hello-0.0.1.war $TEST_DIR/tomcat/
cd ..

# get mod_cluster (Java/Tomcat part)
if [ ! -d mod_cluster ]; then
    git clone https://github.com/modcluster/mod_cluster
fi
cd mod_cluster
git pull --rebase
mvn install || exit 2
cd $TEST_DIR

# prepare jars
mvn install
