#!/usr/bin/sh

. includes/common.sh

httpd_all_clean
tomcat_all_remove

httpd_run || exit 1

# Start 2 tomcats, on 8080 and 8081
tomcat_start_two || exit 1
tomcat_wait_for_n_nodes 2  || exit 1

# Copy testapp and wait for its start
docker cp testapp tomcat8081:/usr/local/tomcat/webapps

# The above statement relies on 'autoDeploy' to be enabled in Tomcat; and while the scan interval of auto deploy is 10 seconds,
# this needs to be adequately higher to propagate the MCMP commands to the reverse proxies in time.
sleep 12

# Basic 200 and 404 tests.
echo "basic 200 and 404 tests"
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp: ${CODE}"
  exit 1
fi
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/toto.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should get 404"
  exit 1
fi

tomcat_all_remove

# Loop stopping starting the same tomcat
ITERATION_COUNT=${ITERATION_COUNT:-10}
iter=0
while [ $iter -lt $ITERATION_COUNT ]
do
   echo "Loop stopping starting the same tomcat iter: $iter"
   nohup docker run --network=host -e tomcat_port=8080 --name tomcat8080 ${IMG} &
   sleep 10
   tomcat_wait_for_n_nodes 1 || exit 1
   docker exec tomcat8080 /usr/local/tomcat/bin/shutdown.sh
   tomcat_wait_for_n_nodes 0 || exit 1
   docker container rm tomcat8080
   iter=$(expr $iter + 1)
done

tomcat_all_remove
