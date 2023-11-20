#!/usr/bin/sh

pwd | grep websocket
if [ $? ]; then
    PREFIX=websocket
else
    PREFIX="."
fi

. includes/common.sh

echo "MPC_NAME: $MPC_NAME"

# remove possibly running containers
httpd_all_clean
tomcat_all_remove

# run a fresh httpd
httpd_run

docker cp $PREFIX/mod_proxy_cluster.conf $MPC_NAME:/usr/local/apache2/conf/mod_proxy_cluster.conf
docker exec $MPC_NAME /usr/local/apache2/bin/apachectl restart

tomcat_start_two || exit 1
tomcat_wait_for_n_nodes 2 || exit 1


# Now try to test the websocket
echo "Testing websocket"
# The websocket-hello app is at: https://github.com/jfclere/httpd_websocket
docker cp websocket/websocket-hello-0.0.1.war tomcat1:/usr/local/tomcat/webapps
docker cp websocket/websocket-hello-0.0.1.war tomcat2:/usr/local/tomcat/webapps

# Put the testapp in the  tomcat we restarted.
docker cp testapp tomcat1:/usr/local/tomcat/webapps
docker cp testapp tomcat2:/usr/local/tomcat/webapps
sleep 12

mvn -f $PREFIX/pom-groovy.xml install
java -jar $PREFIX/target/test-1.0.jar WebSocketsTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with websocket tests"
  exit 1
fi
