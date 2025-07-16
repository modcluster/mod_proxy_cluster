#!/bin/bash

. includes/common.sh

# first stop any previously running tests.
tomcat_all_remove
httpd_remove

# build httpd + mod_proxy_cluster
rm -f nohup.out

MPC_CONF=${MPC_CONF:-MODCLUSTER-734/mod_proxy_cluster.conf}
MPC_NAME=MODCLUSTER-734 httpd_start

# wait until httpd is started
httpd_wait_until_ready || exit 1

sleep 10

# start tomcat1 and tomcat2
tomcat_start_two

# wait until they are in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# copy the test page in ROOT to tomcat8080
docker cp MODCLUSTER-734/ROOT    tomcat1:/usr/local/tomcat/webapps/ROOT
docker cp MODCLUSTER-734/ROOT_OK tomcat2:/usr/local/tomcat/webapps/ROOT

# after a while the health check will get the Under maintenance status.jsp
# and mark the node not OK.
sleep 15

curl -s -m 20 http://localhost:8000/mod_cluster_manager | grep "Status: NOTOK"
if [ $? -eq 0 ]; then
    echo "MODCLUSTER-734 Done!"
else
    echo "MODCLUSTER-734 Failed!"
    tomcat_all_remove
    exit 1
fi

tomcat_all_remove
