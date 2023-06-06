#!/bin/bash
source ../includes/script.bash

# Shell to test MODCLUSTER-640

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
docker stop MODCLUSTER-640
docker rm MODCLUSTER-640

# build httpd + mod_proxy_cluster
rm -f nohup.out
nohup docker run --network=host -e HTTPD=https://dlcdn.apache.org/httpd/httpd-2.4.54.tar.gz -e SOURCES=https://github.com/jfclere/mod_proxy_cluster -e BRANCH=main -e CONF=https://raw.githubusercontent.com/modcluster/mod_proxy_cluster/main/test/native/MODCLUSTER-640/mod_proxy_cluster.conf --name MODCLUSTER-640 quay.io/${USER}/mod_cluster_httpd &

# wait until httpd is started
waitforhttpd  || exit 1

# start 2 tomcats
starttomcats

# wait until the tomcats are in mod_proxy_cluster tables
waitnodes 2

# copy the webapp in the tomcats
docker cp webapp1 tomcat8080:/usr/local/tomcat/webapps/webapp1
docker cp webapp1 tomcat8081:/usr/local/tomcat/webapps/webapp1

sleep 10

# test the URL
code=`/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html`
if [ "${code}" != "200" ]; then
  echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
  exit 1
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -eq 0 ]; then
  echo "nocanon test failed, we get \"jsr:root\"!!!"
  exit 1
fi

# Test without UseNocanon On
sed 's:UseNocanon On::'  mod_proxy_cluster.conf > mod_proxy_cluster_new.conf

docker cp mod_proxy_cluster_new.conf MODCLUSTER-640:/usr/local/apache2/conf/mod_proxy_cluster.conf
docker exec -it  MODCLUSTER-640 /usr/local/apache2/bin/apachectl restart

# wait until the tomcats are back in mod_proxy_cluster tables
waitnodes 2

# test the URL
code=`/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html`
if [ "${code}" != "200" ]; then
  echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
  exit 1
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -ne 0 ]; then
  echo "NO nocanon test failed, we don't get \"jsr:root\"!!!"
  exit 1
fi

# Test for just a proxypass / nocanon
sed 's:UseNocanon On::'  mod_proxy_cluster.conf > mod_proxy_cluster_new.conf
echo "ProxyPass / balancer://mycluster/ nocanon" >> mod_proxy_cluster_new.conf

docker cp mod_proxy_cluster_new.conf MODCLUSTER-640:/usr/local/apache2/conf/mod_proxy_cluster.conf
docker exec -it  MODCLUSTER-640 /usr/local/apache2/bin/apachectl restart

# wait until the tomcats are back in mod_proxy_cluster tables
waitnodes 2

# test the URL
code=`/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html`
if [ "${code}" != "200" ]; then
  echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
  exit 1
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -eq 0 ]; then
  echo "nocanon test failed, we get \"jsr:root\"!!!"
  exit 1
fi
# stop the previous test
#stopprevioustest
