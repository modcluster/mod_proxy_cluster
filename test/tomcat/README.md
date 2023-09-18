# Build the image
```bash
docker build -t tomcat .
```

The default tomcat version on which the image is based is `8.5`. You can
change that during the build by setting `TESTSUITE_TOMCAT_VERSION` variable
to the version you want. For example, for using `tomcat:10.1` as a base,
execute

```
docker build -t tomcat . --build-arg TESTSUITE_TOMCAT_VERSION=10.1
```

# Run image
**Note the ENV variables:**

* tomcat_port: port on which tomcat will listen (default: 8080)
* tomcat_shutdown_port: port on which tomcat will listen to SHUTDOWN command (default 8005)
* tomcat_ajp_port: port on which AJP will be listener (default: 8009)
* cluster_port: port on which the httpd counterpart listens (default: 6666)
* jvm_route: route name of the tomcat
* tomcat_address: ip address of the tomcat

For example:
```bash
docker run --network=host -e tomcat_ajp_port=8010 -e tomcat_address=127.0.0.15 -e jvm_route=tomcat15 --name tomcat15 tomcat
```

then you can uload a webapp into your running instance by executing

```bash
docker cp webapp.war tomcat1:/usr/local/tomcat/webapps/
```
