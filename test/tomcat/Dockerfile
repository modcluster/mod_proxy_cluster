ARG TESTSUITE_TOMCAT_VERSION=8.5

FROM tomcat:${TESTSUITE_TOMCAT_VERSION}

ARG TESTSUITE_TOMCAT_CONFIG=server.xml
ARG TESTSUITE_TOMCAT_CONTEXT=context.xml

# install ss
RUN apt update && apt install -y iproute2

WORKDIR /usr/local/tomcat

COPY target/*.jar ./lib/
COPY *.war ./
COPY $TESTSUITE_TOMCAT_CONFIG  ./conf/server.xml
COPY $TESTSUITE_TOMCAT_CONTEXT ./conf/context.xml
COPY start.sh ./
RUN chmod +x start.sh

ENV cluster_port=6666
ENV jvm_route=
ENV tomcat_address=
ENV tomcat_port_offset=0

CMD ["./start.sh"]

