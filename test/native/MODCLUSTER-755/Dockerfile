FROM tomcat:9.0

WORKDIR /usr/local/tomcat

COPY ./files/server.xml ./conf/
COPY ./files/start.sh ./
RUN chmod +x start.sh
RUN mkdir /usr/local/tomcat/webapps/webapp1
RUN mkdir /usr/local/tomcat/webapps/webapp2
COPY ./files/index.html /usr/local/tomcat/webapps/webapp1
COPY ./files/index.html /usr/local/tomcat/webapps/webapp2


CMD ["./start.sh"]
