#!/usr/bin/sh

. includes/common.sh

httpd_remove
tomcat_all_remove

####################################
###    S T A R T    T E S T S    ###
####################################
httpd_start

# Check that hanging tomcat will be removed
echo "hanging a tomcat checking it is removed after a while no requests"
tomcat_start_two
sleep 10
tomcat_wait_for_n_nodes 2 || exit 1
# curlloop.sh checks for http://localhost:8090/testapp/test.jsp
docker cp testapp tomcat1:/usr/local/tomcat/webapps
docker cp testapp tomcat2:/usr/local/tomcat/webapps
docker cp setenv.sh tomcat1:/usr/local/tomcat/bin

docker commit tomcat1 ${IMG}-debug
tomcat_remove 1
tomcat_wait_for_n_nodes 1
# Start the node.
IMG=${IMG}-debug tomcat_start 1
sleep 10
tomcat_jdbsuspend_prepare 1
tomcat_wait_for_n_nodes 2 || exit 1
echo "2 tomcat started"
# Hang the node,
# jdb and a pipe to hang the tomcat.
tomcat_jdbsuspend_start 1
tomcat_wait_for_n_nodes 1 || exit 1
echo "1 tomcat hanging and gone"
tomcat_jdbsuspend_exit 1
# The tomcat is comming up again
tomcat_wait_for_n_nodes 2 || exit 1
echo "the tomcat is back"

# Same test with requests, make them in a loop
echo "hanging tomcat removed after a while with requests"
sh curlloop.sh 200 000 &
tomcat_jdbsuspend_start 1
tomcat_wait_for_n_nodes 1 || exit 1
ps -ef | grep curlloop | grep -v grep
if [ $? -ne 0 ]; then
    echo "curlloop.sh FAILED!"
    exit 1
fi
ps -ef | grep curlloop | grep -v grep | awk ' { print $2 } ' | xargs kill
tomcat_jdbsuspend_exit 1
# The tomcat is comming up again
tomcat_wait_for_n_nodes 2 || exit 1

# Same test with requets but stop the other tomcat
echo "single hanging tomcat removed after a while with requests"
# kill because we want to keep it in the DNS records
tomcat_kill 2
tomcat_wait_for_n_nodes 1 || exit 1
tomcat_jdbsuspend_start 1
sleep 10
sh curlloop.sh 000 404 503 &
tomcat_wait_for_n_nodes 0 || exit 1
ps -ef | grep curlloop | grep -v grep
if [ $? -ne 0 ]; then
    echo "curlloop.sh FAILED!"
    exit 1
fi
ps -ef | grep curlloop | grep -v grep | awk ' { print $2 } ' | xargs kill
tomcat_jdbsuspend_exit 1
# The tomcat is coming up again
tomcat_wait_for_n_nodes 1 || exit 1

# Cleanup at the end
tomcat_all_remove
tomcat_wait_for_n_nodes 0 || exit 1
