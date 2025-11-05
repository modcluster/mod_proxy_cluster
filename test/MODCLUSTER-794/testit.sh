#!/usr/bin/sh

. includes/common.sh

# first stop any previously running tests.
tomcat_all_remove
httpd_remove

# due to conflict with proxy's 8090 port we shift tomcats
PORT=9000
MPC_CONF=${MPC_CONF:-MODCLUSTER-794/mod_proxy_cluster.conf}
MPC_NAME=MODCLUSTER-794
httpd_start


for i in $(seq 1 20); do
    tomcat_start $i
done

sleep 20

for i in $(seq 1 10); do
    curl -m 10 localhost:8090

    if [ $? -ne 0 ]; then
        echo "curl to server failed"
        tomcat_all_remove
        return 1
    fi
    sleep 5
done

tomcat_wait_for_n_nodes 20

tomcat_all_remove

