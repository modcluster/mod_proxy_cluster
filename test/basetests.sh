#!/usr/bin/sh

. includes/common.sh

httpd_remove
tomcat_all_remove

httpd_start || exit 1

# Start 2 tomcats, on 8080 and 8081
tomcat_start_two || exit 1
tomcat_wait_for_n_nodes 2 || exit 1

# Copy testapp and wait for its start
docker cp testapp tomcat2:/usr/local/tomcat/webapps

# The above statement relies on 'autoDeploy' to be enabled in Tomcat; and while the scan interval of auto deploy is 10 seconds,
# this needs to be adequately higher to propagate the MCMP commands to the reverse proxies in time.
sleep 12

# Basic 200 and 404 tests.
echo "basic 200 and 404 tests"
CODE=$(curl -s -m 20 -o /dev/null -w "%{http_code}" http://localhost:8090/testapp/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp: ${CODE}"
  exit 1
fi
CODE=$(curl -s -m 20 -o /dev/null -w "%{http_code}" http://localhost:8090/testapp/toto.jsp)
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
   tomcat_start 1
   sleep 12
   tomcat_wait_for_n_nodes 1 || exit 1
   tomcat_shutdown 1
   tomcat_wait_for_n_nodes 0 || exit 1
   tomcat_remove 1
   iter=$(expr $iter + 1)
done

tomcat_all_remove
