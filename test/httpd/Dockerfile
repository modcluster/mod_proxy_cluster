FROM fedora:37

RUN yum install gcc wget apr-devel apr-util-devel openssl-devel pcre-devel redhat-rpm-config wcstools git autoconf -y

ENV HTTPD=https://dlcdn.apache.org/httpd/httpd-2.4.57.tar.gz
ENV CONF=httpd/mod_proxy_cluster.conf

# make sure you have copy of the local repository at place
# (our function "httpd_create" takes care of that)
ADD mod_proxy_cluster /
ADD install.sh /tmp

CMD /tmp/install.sh



