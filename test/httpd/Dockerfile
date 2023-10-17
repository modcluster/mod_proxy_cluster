ARG HTTPD_SOURCES="https://dlcdn.apache.org/httpd/httpd-2.4.58.tar.gz"

FROM fedora:38

RUN yum install gcc wget apr-devel apr-util-devel openssl-devel pcre-devel redhat-rpm-config wcstools git autoconf -y

ARG HTTPD_SOURCES

ENV CONF=httpd/mod_proxy_cluster.conf
ENV HTTPD=${HTTPD_SOURCES}

# make sure you have copy of the local repository at place
# (our function "httpd_create" takes care of that)
ADD mod_proxy_cluster /
ADD run.sh /tmp

RUN wget $HTTPD
RUN tar xvf $(filename $HTTPD)
RUN ls
WORKDIR /httpd-2.4.58
RUN ./configure --enable-proxy \
                --enable-proxy-http \
                --enable-proxy-ajp \
                --enable-proxy-wstunnel \
                --enable-proxy-hcheck \
                --with-port=8000
RUN make
RUN make install

# httpd is installed in /usr/local/apache2/bin/
# build and install mod_proxy_cluster *.so files.
WORKDIR /native
RUN for m in advertise mod_proxy_cluster balancers mod_manager; \
    do \
        cd $m; \
        echo "Building $m"; \
        ./buildconf; \
        ./configure --with-apxs=/usr/local/apache2/bin/apxs; \
        make clean; \
        make || exit 1; \
        cp *.so /usr/local/apache2/modules; \
        cd $OLDPWD; \
    done;

CMD /tmp/run.sh

