# tomcat_mod_cluster
Tomcat9 image with mod_cluster enabled.  
Start Apache Httpd with mod cluster enabled.

Make sure you have checkout and build mod_cluster in sub directory at the same level you checkout mod_proxy_cluster.

There are two variables worth setting: `APACHE_BASE` which is by default set to `/usr/local/apache` and which should
correspond to the path where apache is present, then `IMG` which is set by default to
`quay.io/${USER}/tomcat_mod_cluster`.

If your setup differs, set those variables to appropriate values simply by running `export APACHE_BASE=/your/path`.

## Building and pushing the image

You can build the image by running

```
make docker-build
```
and then push it by

```
make docker-push
```

Do not forget to log into quay.io before you run those commands. You can log in using `docker login quay.io`.

If you use `podman` instead of `docker`, you can use `podman-docker` package if it is available for you platform.

## Running the image
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
Tests can be run by invoking `make tests` or manually by running `sh tests.sh` but in that case
make sure you have exported the IMG variable as described above.

The docker image should be build and you might push it before, make sure you have exported the IMG variable.
```
export IMG=quay.io/${USER}/tomcat_mod_cluster
```
# Testing websocket
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

*NOTE: You'll probably need an older JAVA version – version 11 should be ok. You can change it via JAVA env variable.*

# Running tests
You need an Apache httpd with the mod_cluster.so installed and running. You can run it in docker -- checkout the `httpd/`
subdirectory or simply run `make setup-httpd`. You should have following piece in httpd.conf/mod_proxy_cluster.conf:

```
LoadModule manager_module modules/mod_manager.so
LoadModule proxy_cluster_module modules/mod_proxy_cluster.so

ServerName localhost
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
  <Location /mod_cluster_manager>
     Require ip 127.0.0.1
  </Location>
</VirtualHost>
```

Make sure you disable `mod_proxy` module.

You can run tests running `sh tests.sh`. There are a few variables by which you can influence the duration/number of
repetitions (those are printed out with their respective values right after executions starts).

If tests fail or you interupt them, make sure that docker tomcat container that were created are removed first
(you need to run `docker container stop <container>` and `docker container rm <container>`).

# Testing with miniserver
There is also a python script that can be run to check mod_proxy_cluster. You can find it within `includes` directory
as `miniserver.py`. Execute it simply as `./includes/miniserver.py <port-number>`.

You must have `httpd` with `mod_proxy_cluster` running.
