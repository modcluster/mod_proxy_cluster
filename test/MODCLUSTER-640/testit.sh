#!/bin/bash

# Shell to test MODCLUSTER-640

pwd | grep MODCLUSTER-640
if [ $? ]; then
    PREFIX=MODCLUSTER-640
else
    PREFIX="."
fi

. includes/common.sh

# first stop any previously running tests.
tomcat_all_stop
tomcat_all_remove
httpd_all_clean

# build httpd + mod_proxy_cluster
rm -f nohup.out
MPC_CONF=https://raw.githubusercontent.com/modcluster/mod_proxy_cluster/main/test/MODCLUSTER-640/mod_proxy_cluster.conf MPC_NAME=MODCLUSTER-640 httpd_run

# wait until httpd is started
httpd_wait_until_ready  || exit 1


# start 2 tomcats
tomcat_start_two

# wait until the tomcats are in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# copy the webapp in the tomcats
docker cp $PREFIX/webapp1 tomcat8080:/usr/local/tomcat/webapps/webapp1
docker cp $PREFIX/webapp1 tomcat8081:/usr/local/tomcat/webapps/webapp1

sleep 10

# test the URL
code=$(/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html)
if [ "${code}" != "200" ]; then
    echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
    clean_and_exit
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -eq 0 ]; then
    echo "nocanon test failed, we get \"jsr:root\"!!!"
    clean_and_exit
fi

# Test without UseNocanon On
sed 's:UseNocanon On::'  $PREFIX/mod_proxy_cluster.conf > $PREFIX/mod_proxy_cluster_new.conf

docker cp $PREFIX/mod_proxy_cluster_new.conf MODCLUSTER-640:/usr/local/apache2/conf/mod_proxy_cluster.conf
docker exec -it  MODCLUSTER-640 /usr/local/apache2/bin/apachectl restart

# wait until the tomcats are back in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# test the URL
code=$(/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html)
if [ "${code}" != "200" ]; then
    echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
    clean_and_exit
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -ne 0 ]; then
    echo "NO nocanon test failed, we don't get \"jsr:root\"!!!"
    clean_and_exit
fi

# Test for just a proxypass / nocanon
sed 's:UseNocanon On::'  $PREFIX/mod_proxy_cluster.conf > mod_proxy_cluster_new.conf
echo "ProxyPass / balancer://mycluster/ nocanon" >> $PREFIX/mod_proxy_cluster_new.conf

docker cp $PREFIX/mod_proxy_cluster_new.conf MODCLUSTER-640:/usr/local/apache2/conf/mod_proxy_cluster.conf
docker exec -it  MODCLUSTER-640 /usr/local/apache2/bin/apachectl restart

# wait until the tomcats are back in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# test the URL
code=$(/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html)
if [ "${code}" != "200" ]; then
    echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
    clean_and_exit
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -eq 0 ]; then
    echo "nocanon test failed, we get \"jsr:root\"!!!"
    tomcat_all_remove
    clean_and_exit
fi

# clean tomcats
tomcat_all_remove
# and httpd
httpd_all_clean
