#!/bin/sh
#
# Add a web-100 on last node (created to check a regression of MODCLUSTER-765
#
NODE_COUNT="${NODE_COUNT:-500}"
APP_COUNT="${APP_COUNT:-2}"
HTTPD="${HTTPD:-127.0.0.1:6666/}"

i=`expr $NODE_COUNT + 8999`
echo $i
j=100
curl $HTTPD -H "User-Agent: ClusterListener/1.0" -X ENABLE-APP --data "JVMRoute=appserver$i&Alias=default-host%2Clocalhost&Context=%2Fwebapp$j"

