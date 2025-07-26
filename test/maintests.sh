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
httpd_remove
tomcat_all_remove

# run a fresh httpd
httpd_start

tomcat_start_two || exit 1
tomcat_wait_for_n_nodes 2 || exit 1

# Copy testapp and wait for its start
docker cp testapp tomcat2:/usr/local/tomcat/webapps
sleep 12


# Sticky (yes, there is only one app!!!)
echotestlabel "sticky one app"
SESSIONCO=$(curl -v http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${SESSIONCO}" == "" ];then
  echo "Failed no sessionid in curl output..."
  curl -v http://localhost:8090/testapp/test.jsp
fi
echo ${SESSIONCO}
NEWCO=$(curl -v --cookie "${SESSIONCO}" http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${NEWCO}" != "" ]; then
  echo "Failed not sticky received : ${NEWCO}???"
  exit 1
fi

# Copy testapp and wait for starting
docker cp testapp tomcat1:/usr/local/tomcat/webapps
sleep 12

# Sticky (yes there are 2 apps now)
echotestlabel "sticky 2 app"
SESSIONCO=$(curl -v http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
NODE=$(echo ${SESSIONCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
echo "first: ${SESSIONCO} node: ${NODE}"
NEWCO=$(curl -v http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
NEWNODE=$(echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
echo "second: ${NEWCO} node: ${NEWNODE}"
echo "Checking we can reach the 2 nodes"
i=0
while [ "${NODE}" == "${NEWNODE}" ]
do
  NEWCO=$(curl -v http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
  NEWNODE=$(echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
  i=$(expr $i + 1)
  if [ $i -gt 40 ]; then
    echo "Can't find the 2 webapps"
    exit 1
  fi
  if [ "${NEWNODE}" == "" ]; then
    echo "Can't find node in request"
    exit 1
  fi
  echo "trying other webapp try: ${i}"
done
echo "${i} try gives: ${NEWCO} node: ${NEWNODE}"

# Still sticky
CO=$(curl -v --cookie "${SESSIONCO}" http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${CO}" != "" ]; then
  echo "Failed not sticky received : ${CO}???"
  exit 1
fi
CO=$(curl -v --cookie "${NEWCO}" http://localhost:8090/testapp/test.jsp -m 20 -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::')
if [ "${CO}" != "" ]; then
  echo "Failed not sticky received : ${CO}???"
  exit 1
fi

# Stop one of the while running requests.
echotestlabel "sticky: stopping one node and doing requests..."
NODE=$(echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }')
echo $NODE
PORT=$(curl http://localhost:8090/mod_cluster_manager -m 20 | grep Node | grep $NODE | sed 's:)::' | awk -F : '{ print $3 } ')
NAME=$(expr ${PORT} - 8080 + 1)
echo "Will stop tomcat$NAME corresponding to ${NODE} and cookie: ${NEWCO}"
CODE="200"
i=0
while [ "$CODE" == "200" ]
do
  if [ $i -gt 100 ]; then
    echo "Done remaining tomcat still answering!"
    break
  fi
  CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --cookie "${NEWCO}" http://localhost:8090/testapp/test.jsp)
  if [ $i -eq 0 ]; then
    # stop the tomcat
    echo "tomcat${NAME} being stopped"
    tomcat_remove $NAME
  fi
  i=$(expr $i + 1)
done
if [ ${CODE} != "200" ]; then
  echo "Something was wrong... got: ${CODE}"
  curl -v --cookie "${NEWCO}" -m 20 http://localhost:8090/testapp/test.jsp
  exit 1
fi

# Restart the tomcat
tomcat_start ${NAME}
tomcat_wait_for_n_nodes 2

# Test a keepalived connection finds the 2 webapps on each tomcat
echotestlabel "Testing keepalived with 2 webapps on each tomcat"
docker cp testapp tomcat1:/usr/local/tomcat/webapps/testapp1
docker cp testapp tomcat2:/usr/local/tomcat/webapps/testapp2
sleep 12
java -jar includes/target/test-1.0.jar HTTPTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with HTTP tests"
  exit 1
fi

#
# Test virtual host
echotestlabel "Testing virtual hosts"
docker cp tomcat2:/usr/local/tomcat/conf/server.xml .
sed '/Host name=.*/i <Host name=\"example.com\"  appBase="examples" />' server.xml > new.xml
docker cp new.xml tomcat2:/usr/local/tomcat/conf/server.xml
docker cp examples tomcat2:/usr/local/tomcat
docker commit tomcat2 ${IMG}-temporary
tomcat_remove 2
tomcat_wait_for_n_nodes 1
# Start the node.
IMG=${IMG}-temporary tomcat_start 2 &
tomcat_wait_for_n_nodes 2  || exit 1
# Basically curl --header "Host: example.com" http://127.0.0.1:8090/test/test.jsp gives 200
# in fact the headers are:
# X-Forwarded-For: 127.0.0.1
# X-Forwarded-Host: example.com
# X-Forwarded-Server: fe80::faf4:935b:9dda:2adf
# therefore don't forget ProxyPreserveHost On (otherwise UseAlias On failed...)
#
CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: example.com" http://127.0.0.1:8090/test/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't rearch webapp at example.com: ${CODE}"
  exit 1
fi
# Basically curl --header "Host: localhost" http://127.0.0.1:8090/test/test.jsp gives 400
CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: localhost" http://127.0.0.1:8090/test/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT rearch webapp at localhost: ${CODE}"
  exit 1
fi
# Same using localhost/testapp2 and curl --header "Host: localhost" http://127.0.0.1:8090/testapp2/test.jsp
CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: localhost" http://127.0.0.1:8090/testapp2/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't rearch webapp at localhost: ${CODE}"
  exit 1
fi
# Basically curl --header "Host: example.com" http://127.0.0.1:8090/testapp2/test.jsp gives 400
CODE=$(curl -s -o /dev/null -m 20 -w "%{http_code}" --header "Host: example.com" http://127.0.0.1:8090/testapp2/test.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT rearch webapp at localhost: ${CODE}"
  exit 1
fi

# Shutdown the 2 tomcats
tomcat_remove 1
tomcat_remove 2
tomcat_wait_for_n_nodes 0

echotestlabel "Done with the tests!!!"
