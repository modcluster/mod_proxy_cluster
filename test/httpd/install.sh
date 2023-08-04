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
            --with-port=8000 \

make
make install

# httpd is installed in /usr/local/apache2/bin/

# build and install mod_proxy_cluster *.so files.
cd ..
git clone $SOURCES
DIRSOURCES=`filename $SOURCES`
echo "DIRSOURCES: $DIRSOURCES"
cd $DIRSOURCES
# exit if branch does not exist, because main would be used otherwise
git checkout $BRANCH || exit 1
cd ..
for dir in `echo $DIRSOURCES/native/advertise $DIRSOURCES/native/mod_proxy_cluster $DIRSOURCES/native/balancers $DIRSOURCES/native/mod_manager`
do
  cd $dir
  ./buildconf
  ./configure --with-apxs=/usr/local/apache2/bin/apxs
  make clean
  make
  cp *.so /usr/local/apache2/modules
  cd ../../..
done

# wget and copy the prepared conf file and include it
(cd /tmp; wget $CONF)
FILECONF=`filename $CONF`
if [ -f /tmp/$FILECONF ]; then
  cp /tmp/$FILECONF /usr/local/apache2/conf/
  echo "Include conf/$FILECONF" >> /usr/local/apache2/conf/httpd.conf
fi

# start apache httpd server in foreground

/usr/local/apache2/bin/apachectl start
tail -f /usr/local/apache2/logs/error_log
