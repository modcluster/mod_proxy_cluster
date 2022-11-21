#!/bin/bash
source ../includes/script.bash

# check IMG...
if [ -z ${IMG} ]; then
  echo "IMG needs to defined, please try"
  echo "export IMG=quay.io/${USER}/tomcat_mod_cluster"
  exit 1
fi

# first stop any previously running tests.
stoptomcats
removetomcats

# and httpd
podman stop MODCLUSTER-734
podman rm MODCLUSTER-734

# build httpd + mod_proxy_cluster
rm -f nohup.out
nohup podman run --network=host -e HTTPD=https://dlcdn.apache.org/httpd/httpd-2.4.54.tar.gz -e SOURCES=https://github.com/jfclere/mod_proxy_cluster -e BRANCH=main -e CONF=https://raw.githubusercontent.com/modcluster/mod_proxy_cluster/main/test/native/httpd/mod_proxy_cluster.conf --name MODCLUSTER-734 quay.io/${USER}/mod_cluster_httpd &

# wait until httpd is started
waitforhttpd || exit 1
podman cp mod_proxy_cluster.conf MODCLUSTER-734:/usr/local/apache2/conf/mod_proxy_cluster.conf
podman exec -it  MODCLUSTER-734 /usr/local/apache2/bin/apachectl restart

# start tomcat8080 and tomcat8081.
starttomcats

# wait until they are in mod_proxy_cluster tables
waitnodes 2

# copy the test page in ROOT to tomcat8080
podman cp ROOT tomcat8080:/usr/local/tomcat/webapps/ROOT
podman cp ROOT_OK tomcat8081:/usr/local/tomcat/webapps/ROOT

# after a while the health check will get the Under maintenance status.jsp
# and mark the node not OK.
sleep 15
curl -s http://localhost:6666/mod_cluster_manager | grep "Status: NOTOK"
if [ $? -eq 0 ]; then
  echo "MODCLUSTER-734 Done!"
else
  echo "MODCLUSTER-734 Failed!"
  exit 1
fi
