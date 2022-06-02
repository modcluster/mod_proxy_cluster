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


sed -i "s/8080/${tomcat_port}/" ./conf/server.xml

sed -i "s/8005/${tomcat_shutdown_port}/" ./conf/server.xml

sed -i "s/8009/${tomcat_ajp_port}/" ./conf/server.xml

mv  *.war webapps/ || true

if ((${tomcat_port}==0)); then
    sed -i "s/changeMe/${tomcat_ajp_port}/" ./conf/server.xml
else
    sed -i "s/changeMe/${tomcat_port}/" ./conf/server.xml
fi

sed -i "s/6666/${cluster_port}/" ./conf/server.xml

catalina.sh run

