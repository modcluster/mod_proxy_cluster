#!/bin/sh

pwd
ls -lt
echo "HTTPD: $HTTPD"
echo "SOURCES: $SOURCES"
echo "BRANCH: $BRANCH"
echo "CONF: $CONF"
wget $HTTPD
FILE=`filename $HTTPD`
tar xvf $FILE
DIRNAME=`echo $FILE | sed 's:.tar.gz::'`
cd $DIRNAME
./configure --enable-proxy \
            --enable-proxy-http \
            --enable-proxy-ajp \
            --enable-proxy-wstunnel \
            --enable-proxy-hcheck \

make
make install

# httpd is installed in /usr/local/apache2/bin/

# build and install mod_proxy_cluster *.so files.
cd ..
git clone $SOURCES
DIRSOURCES=`filename $SOURCES`
echo "DIRSOURCES: $DIRSOURCES"
cd $DIRSOURCES
git checkout $BRANCH
cd ..
for dir in `echo $DIRSOURCES/native/mod_cluster_slotmem $DIRSOURCES/native/mod_manager $DIRSOURCES/native/mod_proxy_cluster`
do
  cd $dir
  ./buildconf
  ./configure --with-apxs=/usr/local/apache2/bin/apxs
  make
  cp *.so /usr/local/apache2/modules
  cd ../../..
done

# copy one the prepared conf file and include it
cp /tmp/$CONF /usr/local/apache2/conf/
echo "Include conf/$CONF" >> /usr/local/apache2/conf/httpd.conf

# start apache httpd server in foreground

/usr/local/apache2/bin/apachectl start
tail -f /usr/local/apache2/logs/error_log
