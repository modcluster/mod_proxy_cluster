FROM tomcat:9.0

WORKDIR /usr/local/tomcat

COPY ./files/server.xml ./conf/
COPY ./files/start.sh ./
RUN chmod +x start.sh


CMD ["./start.sh"]
