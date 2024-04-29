#!/usr/bin/sh

. includes/common.sh

# remove possibly running containers
httpd_all_clean
tomcat_all_remove

# run a fresh httpd
httpd_run

docker cp websocket/mod_proxy_cluster.conf $MPC_NAME:/usr/local/apache2/conf/mod_proxy_cluster.conf
docker exec $MPC_NAME /usr/local/apache2/bin/apachectl restart

tomcat_start_two || exit 1
tomcat_wait_for_n_nodes 2 || exit 1


# Now try to test the websocket
echo "Testing websocket"
# The websocket-hello app is at: https://github.com/jfclere/httpd_websocket
# we must check whether webapps-javaee exists, if yes, we mut use it becuase the app is javax
docker exec tomcat2 sh -c 'ls webapps-javaee'
if [ $? = 0 ]; then
  docker cp websocket/websocket-hello-0.0.1.war tomcat1:/usr/local/tomcat/webapps-javaee
  docker cp websocket/websocket-hello-0.0.1.war tomcat2:/usr/local/tomcat/webapps-javaee
else
  docker cp websocket/websocket-hello-0.0.1.war tomcat1:/usr/local/tomcat/webapps
  docker cp websocket/websocket-hello-0.0.1.war tomcat2:/usr/local/tomcat/webapps
fi

# Put the testapp in the  tomcat we restarted.
docker cp testapp tomcat1:/usr/local/tomcat/webapps
docker cp testapp tomcat2:/usr/local/tomcat/webapps
sleep 12

java -jar includes/target/test-1.0.jar WebSocketsTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with websocket tests"
  exit 1
fi
