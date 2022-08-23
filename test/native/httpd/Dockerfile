FROM fedora:36

RUN yum install gcc wget apr-devel apr-util-devel openssl-devel pcre-devel redhat-rpm-config wcstools git autoconf -y

ENV HTTPD=https://dlcdn.apache.org/httpd/httpd-2.4.54.tar.gz
ENV SOURCES=https://github.com/modcluster/mod_proxy_cluster
ENV BRANCH=main
ENV CONF=mod_proxy_cluster.conf

ADD install.sh /tmp
ADD *.conf /tmp

CMD /tmp/install.sh



