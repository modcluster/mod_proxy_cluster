#!/usr/bin/sh

. includes/common.sh

httpd_all_clean
tomcat_all_remove

httpd_run

# Start 2 tomcats, on 8080 and 8081
tomcat_start_two || clean_and_exit
tomcat_wait_for_n_nodes 2  || clean_and_exit

# Copy testapp and wait for its start
docker cp testapp tomcat8081:/usr/local/tomcat/webapps
sleep 10

# Basic 200 and 404 tests.
echotestlabel "basic 200 and 404 tests"
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/test.jsp)
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp: ${CODE}"
  clean_and_exit
fi
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/toto.jsp)
if [ ${CODE} != "404" ]; then
  echo "Failed should get 404"
  clean_and_exit
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
   tomcat_wait_for_n_nodes 1 || clean_and_exit
   docker exec -it tomcat8080 /usr/local/tomcat/bin/shutdown.sh
   tomcat_wait_for_n_nodes 0 || clean_and_exit
   docker container rm tomcat8080
   iter=$(expr $iter + 1)
done

tomcat_all_remove
httpd_all_clean
