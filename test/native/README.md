# tomcat_mod_cluster
Tomcat9 image with mod_cluster enabled.  
Start Apache Httpd with mod cluster enabled.

Make sure you have checkout and build mod_cluster in sub directory at the same level you checkout mod_proxy_cluster.

## Build the image with:
```
export IMG=quay.io/${USER}/tomcat_mod_cluster
make docker-build
```

## Push to quay.io:
```
export IMG=quay.io/${USER}/tomcat_mod_cluster
make docker-push
```

## Run the image with:  
```
docker run --network=host -e tomcat_port=[port1] -e cluster_port=[port3] [image]
Or
docker run --network=host -e tomcat_ajp_port=[port1] -e cluster_port=[port3] [image]
# You can also add the variable -e tomcat_shutdown_port=true if u want to have a shutdown port
```

To load webapps into the container:
```
docker cp webapp.war <containerName>:/usr/local/tomcat/webapps/
```
# mod_cluster_tests
Tests using docker/podman to test mod_cluster

The docker image should be build and you might push it before, make sure you have exported the IMG variable.
```
export IMG=quay.io/${USER}/tomcat_mod_cluster
```
# testing websocket
Using com.ning.http.client.ws.WebSocketTextListener
To be able to run the test please use https://github.com/jfclere/httpd_websocket just build it:
```
git clone https://github.com/jfclere/httpd_websocket
cd https://github.com/jfclere/httpd_websocket
mvn install
cd ..
```
Build the groovy jar
```
mvn install
```
run the groovy stuff
```
java -jar target/test-1.0.jar
```

# run the tests
you need an Apache httpd with the mod_cluster.so installed and the following piece in httpd.conf
```
LoadModule cluster_slotmem_module modules/mod_cluster_slotmem.so
LoadModule manager_module modules/mod_manager.so
LoadModule proxy_cluster_module modules/mod_proxy_cluster.so

  Listen 6666
  ManagerBalancerName mycluster
  EnableWsTunnel
  WSUpgradeHeader "websocket"
  <VirtualHost *:6666>
   <Directory />
       Require ip 127.0.0.1
    </Directory>

    KeepAliveTimeout 300
    MaxKeepAliveRequests 0

    EnableMCPMReceive
    ServerName localhost

    <Location /mod_cluster_manager>
       Require ip 127.0.0.1
    </Location>
  </VirtualHost>
```
