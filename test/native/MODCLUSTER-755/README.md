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
docker run [image]
```

## Start test
```
bash mcmps.sh
```
