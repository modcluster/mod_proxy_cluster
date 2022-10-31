# tomcat_mod_cluster
Test for MODCLUSTER-755

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
docker run --network host --cgroups=disabled [image]
```

## Start test

Make sure mod_proxy_cluster has enough context, alias and nodes:
```
  Maxnode 505
  Maxhost 1010
  Maxcontext 1010
```

Start the script:
```
bash mcmps.sh
```


Note: The variables for the script, NODE_COUNT, APP_COUNT and HTTPD, defaulting to 500 2 and "127.0.0.1:6666/"
```
NODE_COUNT=250 APP_COUNT=4 HTTPD=127.0.0.1:8888/ bash mcmps.sh
```

Note: The script enable 2 webapps (webapp1 webapp2)
