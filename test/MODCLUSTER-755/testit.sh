#!/bin/sh
#
# NODE_COUNT - number of nodes to generate
# APP_COUNT - number of apps to generate
# HTTPD - destination to connect to for httpd/mod_cluster
# USE_MULTI_APP - if true different webapps are created for each node.
#

. includes/common.sh

httpd_remove
tomcat_all_remove

MPC_CONF=MODCLUSTER-755/mod_proxy_cluster.conf MPC_NAME=MODCLUSTER-755 httpd_start

httpd_wait_until_ready

tomcat_start 1

NODE_COUNT="${NODE_COUNT:-500}"
APP_COUNT="${APP_COUNT:-2}"
HTTPD="${HTTPD:-127.0.0.1:6666/}"
USE_MULTI_APP="${USE_MULTI_APP:-false}"

echo "NODE_COUNT: ${NODE_COUNT}"
echo "APP_COUNT: ${APP_COUNT}"
echo "Apache HTTPD MCMP URL: ${HTTPD}"
echo "USE_MULTI_APP: $USE_MULTI_APP"

if [ "x$USE_MULTI_APP" = "xtrue" ]; then
  echo "The webapp are going to be 1-9000/2-9000 until count (1-9499/2-9499)"
fi

for i in $(seq 9000 $(expr 9000 + $NODE_COUNT - 1))
do
   curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X CONFIG --data "JVMRoute=appserver$i&Host=127.0.0.1&Maxattempts=1&Port=$i&StickySessionForce=No&Timeout=20&Type=ajp&ping=20"
   curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X STATUS --data "JVMRoute=appserver$i&Load=100"

   for j in $(seq 1 $APP_COUNT)
   do
      if [ "x$USE_MULTI_APP" = "xtrue" ]; then
         curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X ENABLE-APP --data "JVMRoute=appserver$i&Alias=default-host%2Clocalhost&Context=%2Fwebapp$j-$i"
      else
         curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X ENABLE-APP --data "JVMRoute=appserver$i&Alias=default-host%2Clocalhost&Context=%2Fwebapp$j"
      fi
   done
done

i=0
while [ true ]
do
   for i in $(seq 9000 $(expr 9000 + $NODE_COUNT - 1))
   do
      curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X STATUS --data "JVMRoute=appserver$i&Load=100"
      if [ $? -ne 0 ]; then
        echo "htttpd stopped!!!"
        exit 1
      fi
   done
   sleep 10
   i=$(expr $i + 1)
   if [ $i -gt 100 ]; then
      break
   fi
done

tomcat_all_remove
