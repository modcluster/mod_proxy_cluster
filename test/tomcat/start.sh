#!/bin/bash

sed -i "s/tomcat_address/${tomcat_address:-127.0.0.1}/g" ./conf/server.xml
sed -i "s/port_offset/${tomcat_port_offset:-0}/g" ./conf/server.xml
sed -i "s/proxy_port/${cluster_port:-6666}/g" ./conf/server.xml
sed -i "s/proxy_address/${cluster_address:-127.0.0.1}/g" ./conf/server.xml
sed -i "s/proxy_address/${cluster_address:-127.0.0.1}/g" ./conf/context.xml

conPort=$(expr 8080 + ${tomcat_port_offset:-0})
sed -i "s/changeMe/$conPort/" ./conf/server.xml

if [ ! -z ${jvm_route} ]; then
  sed -i "/<Engine name=\"Catalina\"/c\<Engine name=\"Catalina\" defaultHost=\"localhost\" jvmRoute=\"${jvm_route}\">" ./conf/server.xml
fi

# copy webapp war file.
mv *.war webapps/ || true

catalina.sh run

