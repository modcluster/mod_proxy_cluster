ARG TESTSUITE_TOMCAT_VERSION=8.5

FROM tomcat:${TESTSUITE_TOMCAT_VERSION}

# install ss
RUN apt update && apt install -y iproute2

WORKDIR /usr/local/tomcat

COPY target/*.jar ./lib/
COPY server.xml ./conf/
COPY context.xml ./conf/
COPY start.sh ./
RUN chmod +x start.sh


ENV tomcat_port=
ENV tomcat_shutdown_port=
ENV tomcat_ajp_port=
ENV cluster_port=6666
ENV jvm_route=
ENV tomcat_address=

CMD ["./start.sh"]

