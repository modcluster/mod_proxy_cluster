# Build the image
```bash
docker build -t httpd .
```

# run it
**Note the ENV variables:**

* HTTPD: URL to a released httpd tar.gz
* SOURCES: The mod_proxy_sources, url (github)
* BRANCH: A branch, or a tag or a commitid
* CONF: The mod_proxy_cluster configuration file to include in httpd.conf (The *conf files are added to image at build time)

For example (the default):
```bash
docker run -d --network=host -e HTTPD=https://dlcdn.apache.org/httpd/httpd-2.4.57.tar.gz -e SOURCES=https://github.com/modcluster/mod_proxy_cluster -e BRANCH=main -e CONF=https://raw.githubusercontent.com/modcluster/mod_proxy_cluster/main/test/httpd/mod_proxy_cluster.conf httpd
```

# Test with different configs without rebuilding httpd or mod_proxy_cluster
```bash
docker ps
```
then gives something like
```
[jfclere@fedora test]$ docker ps
CONTAINER ID  IMAGE                                     COMMAND               CREATED      STATUS          PORTS       NAMES
f76915af248f  httpd:latest  /bin/sh -c /tmp/i...  2 hours ago  Up 2 hours ago              condescending_moser
```
copy your mod_proxy_cluster.conf file
```
docker cp mod_proxy_cluster.conf condescending_moser:/usr/local/apache2/conf/mod_proxy_cluster.conf
```
then restart httpd:
```
docker exec -it  condescending_moser /usr/local/apache2/bin/apachectl restart
```
