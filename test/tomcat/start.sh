#!/bin/bash

if [ "$tomcat_shutdown_port" = true ] 
then
    while :
    do
        port=$(shuf -i 8000-9000 -n 1)
        if [[ $(ss -tulpn | grep $port) ]] || [[ "$port" = $tomcat_port ]] || [[ "$port" = $tomcat_ajp_port ]]; then
            echo "Port $port is used"
        else
            tomcat_shutdown_port=$port
            break
        fi
    done
fi


if [ ! -z ${tomcat_port} ]; then
  sed -i "s/8080/${tomcat_port}/" ./conf/server.xml
fi

if [ ! -z ${tomcat_shutdown_port} ]; then
  sed -i "s/8005/${tomcat_shutdown_port}/" ./conf/server.xml
fi

if [ ! -z ${tomcat_ajp_port} ]; then
  sed -i "s/8009/${tomcat_ajp_port}/" ./conf/server.xml
fi
if [ ! -z ${tomcat_address} ]; then
  sed -i "s/127.0.0.1/${tomcat_address}/" ./conf/server.xml
fi

if [ -z ${tomcat_port} ]; then
  if [ ! -z ${tomcat_ajp_port} ]; then
    sed -i "s/changeMe/${tomcat_ajp_port}/" ./conf/server.xml
  else
    sed -i "s/changeMe/8009/" ./conf/server.xml
  fi
else
    sed -i "s/changeMe/${tomcat_port}/" ./conf/server.xml
fi


if [ "${cluster_port}" -eq "0" ]; then
  cluster_port=6666
fi
sed -i "s/proxyport/${cluster_port}/" ./conf/server.xml
sed -i "s/proxyaddress/127.0.0.1/" ./conf/server.xml

echo "jvm_route: ${jvm_route} and tomcat_port: ${tomcat_port} and tomcat_address: ${tomcat_address}"
if [ ! -z ${jvm_route} ]; then
  sed -i "/<Engine name=\"Catalina\"/c\<Engine name=\"Catalina\" defaultHost=\"localhost\" jvmRoute=\"${jvm_route}\">" ./conf/server.xml
fi

# copy webapp war file.
mv *.war webapps/ || true

catalina.sh run

