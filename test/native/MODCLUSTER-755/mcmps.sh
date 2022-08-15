#!/bin/sh
#
# NODE_COUNT - number of nodes to generate
# APP_COUNT - number of apps to generate
# HTTPD - destination to connect to for httpd/mod_cluster
#

NODE_COUNT="${NODE_COUNT:-500}"
APP_COUNT="${APP_COUNT:-2}"
HTTPD="${HTTPD:-127.0.0.1:6666/}"

echo "NODE_COUNT: ${NODE_COUNT}"
echo "APP_COUNT: ${APP_COUNT}"
echo "Apache HTTPD MCMP URL: ${HTTPD}"

for ((i=9000; i < `expr 9000+$NODE_COUNT`; i++))
do
   curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X CONFIG --data "JVMRoute=appserver$i&Host=127.0.0.1&Maxattempts=1&Port=$i&StickySessionForce=No&Timeout=20&Type=ajp&ping=20"
   curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X STATUS --data "JVMRoute=appserver$i&Load=100"

   for ((j=1; j <= $APP_COUNT; j++))
   do
      curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X ENABLE-APP --data "JVMRoute=appserver$i&Alias=default-host%2Clocalhost&Context=%2Fwebapp$j"
   done
done

while [ true ]
do
   for ((i=9000; i < 9000+$NODE_COUNT; i++))
   do
      curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X STATUS --data "JVMRoute=appserver$i&Load=100"
   done
   sleep 10
done
