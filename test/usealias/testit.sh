#!/usr/bin/sh

. includes/common.sh

# remove possibly running containers
httpd_remove
tomcat_all_remove

# Make sure to use custom conf with UseAlias On
MPC_CONF=usealias/mod_proxy_cluster.conf httpd_start


tomcat_start 1

tomcat_wait_for_n_nodes 1

# Test virtual host
echo "Create a temporary image with a custom server.xml"

docker cp usealias/server.xml tomcat1:/usr/local/tomcat/conf/server.xml
# This app will have alias = example.com
docker cp usealias/examples tomcat1:/usr/local/tomcat
# This app will have two aliases: localhost, tomcat1
docker cp usealias/examples/test tomcat1:/usr/local/tomcat/webapps/webapp
docker commit tomcat1 ${IMG}-temporary
# TODO: Without shutdown only the examples.com VHost is available, looks fishy
tomcat_shutdown 1

tomcat_wait_for_n_nodes 0
tomcat_remove 1

# Start the node.
echo "Start custom tomcat image as tomcat1"
IMG=${IMG}-temporary tomcat_start 1

tomcat_wait_for_n_nodes 1
sleep 20

# Let's test now that
# curl --header "Host: example.com" http://localhost:8090/test/test.jsp
# gives 200
# in fact the headers are:
# X-Forwarded-For: localhost
# X-Forwarded-Host: example.com
#
# Don't forget ProxyPreserveHost On (otherwise UseAlias On failed...)
#
# Basically each Host header value is compared with the aliases of the VHost corresponding
# to the request and UseAlias makes sure there's a match (if not, 404 is returned)

CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: example.com" http://localhost:8090/test/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp at example.com: ${CODE}"
  exit 1
fi

CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: localhost" http://localhost:8090/test/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT reach webapp at localhost: ${CODE}"
  exit 1
fi

CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: tomcat1" http://localhost:8090/test/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT reach webapp at localhost: ${CODE}"
  exit 1
fi

CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: localhost" http://localhost:8090/webapp/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp at localhost: ${CODE}"
  exit 1
fi

CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: tomcat1" http://localhost:8090/webapp/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp at localhost: ${CODE}"
  exit 1
fi

CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: example.com" http://localhost:8090/webapp/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT reach webapp at localhost: ${CODE}"
  exit 1
fi

tomcat_all_remove
docker image rm ${IMG}-temporary
