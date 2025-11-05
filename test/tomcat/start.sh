#!/bin/bash

if [ ! -z ${jvm_route} ]; then
  sed -i "/<Engine name=\"Catalina\"/c\    <Engine name=\"Catalina\" defaultHost=\"tomcat_address\" jvmRoute=\"${jvm_route}\">" ./conf/server.xml
fi

sed -i "s/tomcat_address/${tomcat_address}/g" ./conf/server.xml
sed -i "s/tomcat_port/${tomcat_port}/g" ./conf/server.xml
sed -i "s/tomcat_shutdown_port/${tomcat_shutdown_port}/g" ./conf/server.xml
sed -i "s/tomcat_ajp_port/${tomcat_ajp_port}/g" ./conf/server.xml
sed -i "s/port_offset/${tomcat_port_offset}/g" ./conf/server.xml
sed -i "s/proxy_port/${proxy_port}/g" ./conf/server.xml
sed -i "s/proxy_address/${proxy_address}/g" ./conf/server.xml

ls lib/jakartaee-migration-*.jar
if [ $? = 0 ]; then
  mkdir webapps-javaee
fi

# spawn the tomcat in a separate shell
bin/catalina.sh run
# just stay around even when the tomcat process gets killed
while true; do cat /dev/null; done;
