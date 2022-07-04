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
docker run --network host [image]
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
