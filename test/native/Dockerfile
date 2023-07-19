FROM tomcat:8.5

WORKDIR /usr/local/tomcat

COPY target/*.jar ./lib/
COPY ./files/server.xml ./conf/
COPY ./files/context.xml ./conf/
COPY ./files/start.sh ./
RUN chmod +x start.sh


ENV tomcat_port=
ENV tomcat_shutdown_port=
ENV tomcat_ajp_port=
ENV cluster_port=6666
ENV jvm_route=
ENV tomcat_address=

CMD ["./start.sh"]



