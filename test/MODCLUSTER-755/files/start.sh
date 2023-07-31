#!/bin/bash

#
# NODE_COUNT - number of connectors to generate for simulated nodes
#

NODE_COUNT=1000

for i in $(seq 0 $NODE_COUNT)
do
   PORT=`expr $i + 8999`
   echo "    <Connector protocol=\"AJP/1.3\" address=\"127.0.0.1\" port=\"$PORT\" secretRequired=\"false\" />" >>add.txt
done
sed -i '/Service name/r add.txt' ./conf/server.xml


catalina.sh run

