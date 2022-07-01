IMG ?= tomcat_mod_cluster:latest

docker-build:
	docker build -t ${IMG} .

docker-push: ## Push docker image with the manager.
	docker push ${IMG}
