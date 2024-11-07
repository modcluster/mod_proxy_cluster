#!/bin/bash

. includes/common.sh

# first stop any previously running tests.
tomcat_all_remove
httpd_remove

# build httpd + mod_proxy_cluster
rm -f nohup.out

MPC_CONF=${MPC_CONF:-MODCLUSTER-640/mod_proxy_cluster.conf}
MPC_NAME=MODCLUSTER-640 httpd_start

# wait until httpd is started
httpd_wait_until_ready  || exit 1


# start 2 tomcats
tomcat_start_two

# wait until the tomcats are in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# copy the webapp in the tomcats
docker cp MODCLUSTER-640/webapp1 tomcat1:/usr/local/tomcat/webapps/webapp1
docker cp MODCLUSTER-640/webapp1 tomcat2:/usr/local/tomcat/webapps/webapp1

sleep 12

# test the URL
code=$(/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html)
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
docker exec MODCLUSTER-640 sh -c "sed -i 's:UseNocanon On::' /usr/local/apache2/conf/$(filename $MPC_CONF)"

docker exec MODCLUSTER-640 /usr/local/apache2/bin/apachectl restart

# wait until the tomcats are back in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# test the URL
code=$(/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html)
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
docker exec MODCLUSTER-640 sh -c "echo 'ProxyPass / balancer://mycluster/ nocanon' >> /usr/local/apache2/conf/$(filename $MPC_CONF)"
docker exec MODCLUSTER-640 /usr/local/apache2/bin/apachectl restart

# wait until the tomcats are back in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 2

# test the URL
code=$(/usr/bin/curl -o /dev/null --silent --write-out '%{http_code}' http://localhost:8000/webapp1/index.html)
if [ "${code}" != "200" ]; then
    echo "nocanon test failed, we get ${code} on http://localhost:8000/webapp1/index.html"
    exit 1
fi
curl -v "http://localhost:8000/webapp1/jsr%3aroot/toto" | grep "jsr:root"
if [ $? -eq 0 ]; then
    echo "nocanon test failed, we get \"jsr:root\"!!!"
    tomcat_all_remove
    exit 1
fi

# clean tomcats
tomcat_all_remove
