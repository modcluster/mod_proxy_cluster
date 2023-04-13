APACHE_BASE ?= /usr/local/apache
APACHE_INC  ?= ${APACHE_BASE}/include
IMG ?= quay.io/${USER}/tomcat_mod_cluster:latest
MOD_CLUSTER_VERSION ?= 2.0.2.Final-SNAPSHOT

Advertise: Advertise.c
	cc -c -I$(APACHE_INC) Advertise.c
	cc -o Advertise Advertise.o -L$(APACHE_BASE)/lib -lapr-1

jars:
	mvn dependency:copy-dependencies

docker-build: jars
	docker build -t ${IMG} .

docker-push: ## Push docker image with the manager.
	docker push ${IMG}

setup-httpd:
	cd httpd
	podman build -t quay.io/${USER}/mod_cluster_httpd .
	podman run -d --network=host quay.io/${USER}/mod_cluster_httpd
	cd ..

tests:
	IMG=${IMG} sh tests.sh
