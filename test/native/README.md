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
