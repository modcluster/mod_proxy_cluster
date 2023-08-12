#!/bin/sh

pwd
ls -lt
echo "HTTPD: $HTTPD"
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
cd /native
for m in advertise mod_proxy_cluster balancers mod_manager
do
  cd $m
  echo "Building $m"
  ./buildconf
  ./configure --with-apxs=/usr/local/apache2/bin/apxs
  make clean
  make || exit 1
  cp *.so /usr/local/apache2/modules
  cd $OLDPWD
done

# wget and copy the prepared conf file and include it
cd /test/
if [ -f $CONF ]; then
  FILECONF=$(filename $CONF)
  cp $CONF /usr/local/apache2/conf/
  echo "Include conf/$FILECONF" >> /usr/local/apache2/conf/httpd.conf
else
  echo "The given CONF file: $CONF does not exist"
  exit 1
fi

# start apache httpd server in foreground
echo "Starting httpd..."
/usr/local/apache2/bin/apachectl start
tail -f /usr/local/apache2/logs/error_log
