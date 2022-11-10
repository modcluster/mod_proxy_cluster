# build the image
```bash
podman build -t quay.io/${USER}/mod_cluster_httpd .
```

# run it
**Note the ENV variables:**

HTTPD: URL to a released httpd tar.gz

SOURCES: The mod_proxy_sources, url (github)

BRANCH: A branch, or a tag or a commitid

CONF: The mod_proxy_cluster configuration file to include in httpd.conf (The *conf files are added to image at build time)

For example (the default)
```bash
podman run --network=host -e HTTPD=https://dlcdn.apache.org/httpd/httpd-2.4.54.tar.gz -e SOURCES=https://github.com/modcluster/mod_proxy_cluster/ -e BRANCH=main -e CONF=mod_proxy_cluster.conf quay.io/${USER}/mod_cluster_httpd
```
