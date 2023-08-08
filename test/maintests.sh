#!/usr/bin/sh

. includes/common.sh

# Echo test label message in order to know where we are
echotestlabel() {
    echo "***************************************************************"
    echo "Doing test: $1"
    echo "***************************************************************"
}

# Main piece -- tests start here
echotestlabel "Starting tests!!!"

# remove possibly running containers
httpd_all_clean
tomcat_all_remove

# run a fresh httpd
httpd_run

tomcat_start_two || clean_and_exit
tomcat_wait_for_n_nodes 2 || clean_and_exit

# Copy testapp and wait for its start
docker cp testapp tomcat8081:/usr/local/tomcat/webapps
sleep 10


# Sticky (yes, there is only one app!!!)
echotestlabel "sticky one app"
SESSIONCO=$(curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${SESSIONCO}" == "" ];then
  echo "Failed no sessionid in curl output..."
  curl -v http://localhost:8000/testapp/test.jsp
fi
echo ${SESSIONCO}
NEWCO=$(curl -v --cookie "${SESSIONCO}" http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${NEWCO}" != "" ]; then
  echo "Failed not sticky received : ${NEWCO}???"
  clean_and_exit
fi

# Copy testapp and wait for starting
docker cp testapp tomcat8080:/usr/local/tomcat/webapps
sleep 10

# Sticky (yes there are 2 apps now)
echotestlabel "sticky 2 app"
SESSIONCO=$(curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
NODE=$(echo ${SESSIONCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
echo "first: ${SESSIONCO} node: ${NODE}"
NEWCO=$(curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
NEWNODE=$(echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
echo "second: ${NEWCO} node: ${NEWNODE}"
echo "Checking we can reach the 2 nodes"
i=0
while [ "${NODE}" == "${NEWNODE}" ]
do
  NEWCO=$(curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
  NEWNODE=$(echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
  i=$(expr $i + 1)
  if [ $i -gt 40 ]; then
    echo "Can't find the 2 webapps"
    clean_and_exit
  fi
  if [ "${NEWNODE}" == "" ]; then
    echo "Can't find node in request"
    clean_and_exit
  fi
  echo "trying other webapp try: ${i}"
  clean_and_exit
done
echo "${i} try gives: ${NEWCO} node: ${NEWNODE}"

# Still sticky
CO=$(curl -v --cookie "${SESSIONCO}" http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${CO}" != "" ]; then
  echo "Failed not sticky received : ${CO}???"
  clean_and_exit
fi
CO=$(curl -v --cookie "${NEWCO}" http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${CO}" != "" ]; then
  echo "Failed not sticky received : ${CO}???"
  clean_and_exit
fi

# Stop one of the while running requests.
echotestlabel "sticky: stopping one node and doing requests..."
NODE=$(echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
echo $NODE
PORT=$(curl http://localhost:6666/mod_cluster_manager | grep Node | grep $NODE | sed 's:)::' | awk -F : '{ print $3 } ')
echo "Will stop ${PORT} corresponding to ${NODE} and cookie: ${NEWCO}"
CODE="200"
i=0
while [ "$CODE" == "200" ]
do
  if [ $i -gt 100 ]; then
    echo "Done remaining tomcat still answering!"
    break
  fi
  CODE=$(curl -s -o /dev/null -w "%{http_code}" --cookie "${NEWCO}" http://localhost:8000/testapp/test.jsp)
  if [ $i -eq 0 ]; then
    # stop the tomcat
    echo "tomcat${PORT} being stopped"
    docker stop tomcat${PORT}
    docker container rm tomcat${PORT}
  fi
  i=$(expr $i + 1)
done
if [ ${CODE} != "200" ]; then
  echo "Something was wrong... got: ${CODE}"
  curl -v --cookie "${NEWCO}" http://localhost:8000/testapp/test.jsp
  clean_and_exit
fi

# Restart the tomcat
nohup docker run --network=host -e tomcat_port=${PORT} -e tomcat_shutdown_port=true --name tomcat${PORT} ${IMG} &

# Now try to test the websocket
echotestlabel "testing websocket"
# The websocket-hello app is at: https://github.com/jfclere/httpd_websocket
mvn dependency:copy -U -Dartifact=org.apache.tomcat:websocket:hello:0.0.1:war  -DoutputDirectory=.
if [ $? -ne 0 ]; then
  echo "Something was wrong... can't find org.apache.tomcat:websocket:hello:0.0.1:war"
  cp $HOME/.m2/repository/org/apache/tomcat/websocket-hello/0.0.1/websocket-hello-0.0.1.war .
  if [ $? -ne 0 ]; then
    clean_and_exit
  fi
fi
docker cp websocket-hello-0.0.1.war tomcat8080:/usr/local/tomcat/webapps
docker cp websocket-hello-0.0.1.war tomcat8081:/usr/local/tomcat/webapps
# Put the testapp in the  tomcat we restarted.
docker cp testapp tomcat${PORT}:/usr/local/tomcat/webapps
sleep 10
mvn -f pom-groovy.xml install
java -jar target/test-1.0.jar WebSocketsTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with websocket tests"
  clean_and_exit
fi

#
# Test a keepalived connection finds the 2 webapps on each tomcat
echotestlabel "Testing keepalived with 2 webapps on each tomcat"
docker cp testapp tomcat8080:/usr/local/tomcat/webapps/testapp1
docker cp testapp tomcat8081:/usr/local/tomcat/webapps/testapp2
sleep 10
java -jar target/test-1.0.jar HTTPTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with HTTP tests"
  clean_and_exit
fi

#
# Test virtual host
echotestlabel "Testing virtual hosts"
docker cp tomcat8081:/usr/local/tomcat/conf/server.xml .
sed '/Host name=.*/i <Host name=\"example.com\"  appBase="examples" />' server.xml > new.xml
docker cp new.xml tomcat8081:/usr/local/tomcat/conf/server.xml
docker cp examples tomcat8081:/usr/local/tomcat
docker commit tomcat8081 ${IMG}-temporary
docker stop tomcat8081
docker container rm tomcat8081
tomcat_wait_for_n_nodes 1
# Start the node.
nohup docker run --network=host -e tomcat_port=8081 -e tomcat_shutdown_port=true --name tomcat8081 ${IMG}-temporary &
tomcat_wait_for_n_nodes 2  || exit 1
# Basically curl --header "Host: example.com" http://127.0.0.1:8000/test/test.jsp gives 200
# in fact the headers are:
# X-Forwarded-For: 127.0.0.1
# X-Forwarded-Host: example.com
# X-Forwarded-Server: fe80::faf4:935b:9dda:2adf
# therefore don't forget ProxyPreserveHost On (otherwise UseAlias On failed...)
#
CODE=$(curl -s -o /dev/null -w "%{http_code}" --header "Host: example.com" http://127.0.0.1:8000/test/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't rearch webapp at example.com: ${CODE}"
  clean_and_exit
fi
# Basically curl --header "Host: localhost" http://127.0.0.1:8000/test/test.jsp gives 400
CODE=$(curl -s -o /dev/null -w "%{http_code}" --header "Host: localhost" http://127.0.0.1:8000/test/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT rearch webapp at localhost: ${CODE}"
  clean_and_exit
fi
# Same using localhost/testapp2 and curl --header "Host: localhost" http://127.0.0.1:8000/testapp2/test.jsp
CODE=$(curl -s -o /dev/null -w "%{http_code}" --header "Host: localhost" http://127.0.0.1:8000/testapp2/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't rearch webapp at localhost: ${CODE}"
  clean_and_exit
fi
# Basically curl --header "Host: example.com" http://127.0.0.1:8000/testapp2/test.jsp gives 400
CODE=$(curl -s -o /dev/null -w "%{http_code}" --header "Host: example.com" http://127.0.0.1:8000/testapp2/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT rearch webapp at localhost: ${CODE}"
  exit 1
fi

# Shutdown the 2 tomcats
docker exec -it tomcat8080 /usr/local/tomcat/bin/shutdown.sh
docker exec -it tomcat8081 /usr/local/tomcat/bin/shutdown.sh
tomcat_wait_for_n_nodes 0
docker container rm tomcat8080
docker container rm tomcat8081

echotestlabel "Done with the tests!!!"
