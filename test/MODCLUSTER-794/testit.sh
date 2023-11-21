#!/usr/bin/sh

. includes/common.sh

# first stop any previously running tests.
tomcat_all_stop
tomcat_all_remove
httpd_all_clean

MPC_NAME=MODCLUSTER-794 MPC_CONF=MODCLUSTER-794/mod_proxy_cluster.conf httpd_run

for i in {1..20}; do
    tomcat_start $i
done

sleep 20

for i in $(seq 1 10); do
    curl -m 10 localhost:6666

    if [ $? -ne 0 ]; then
        echo "curl to server failed"
        tomcat_all_remove
        return 1
    fi
    sleep 5
done

tomcat_all_remove

