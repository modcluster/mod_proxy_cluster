#!/bin/bash

sed -i "s/tomcat_address/${tomcat_address:-127.0.0.1}/g" ./conf/server.xml
sed -i "s/port_offset/${tomcat_port_offset:-0}/g" ./conf/server.xml
sed -i "s/proxy_port/${cluster_port:-8090}/g" ./conf/server.xml
sed -i "s/proxy_address/${cluster_address:-127.0.0.1}/g" ./conf/server.xml
sed -i "s/proxy_address/${cluster_address:-127.0.0.1}/g" ./conf/context.xml

if [ ! -z ${jvm_route} ]; then
  sed -i "/<Engine name=\"Catalina\"/c\<Engine name=\"Catalina\" defaultHost=\"localhost\" jvmRoute=\"${jvm_route}\">" ./conf/server.xml
fi

ls lib/jakartaee-migration-*.jar
if [ $? = 0 ]; then
  rm lib/mod_cluster-container-tomcat-9.0-*.Final-SNAPSHOT.jar
  mkdir webapps-javaee
else
  rm lib/mod_cluster-container-tomcat-10.1-*.Final-SNAPSHOT.jar
fi

bin/catalina.sh run
