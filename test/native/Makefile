APACHE_BASE ?= /home/jbento/modclusterdemo/opt/jboss/httpd/
APACHE_INC  ?= /home/jbento/modclusterdemo/opt/jboss/httpd/include/httpd
IMG ?= tomcat_mod_cluster:latest
MOD_CLUSTER_VERSION ?= 2.0.0.Alpha1-SNAPSHOT

Advertise: Advertise.c
	cc -c -I$(APACHE_INC) Advertise.c
	cc -o Advertise Advertise.o -L$(APACHE_BASE)/lib -lapr-1

jars:
	mvn dependency:copy-dependencies

docker-build: jars
	docker build -t ${IMG} .

docker-push: ## Push docker image with the manager.
	docker push ${IMG}
