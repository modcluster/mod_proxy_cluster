#!/bin/sh

# copy the prepared conf file and include it
cd /test/
FILECONF=$(filename $CONF)
if [ -f $CONF ]; then
  cp $CONF /usr/local/apache2/conf/
  echo "Include conf/$FILECONF" >> /usr/local/apache2/conf/httpd.conf
else
  echo "The given CONF file: $CONF does not exist"
  exit 1
fi

if [ ! -z "$MPC_NAME" ]; then
    sed -i "s/ServerName httpd-mod_proxy_cluster/ServerName ${MPC_NAME}/g" /usr/local/apache2/conf/$FILECONF
fi

# start apache httpd server in foreground
echo "Starting httpd..."
/usr/local/apache2/bin/apachectl start
tail -f /usr/local/apache2/logs/error_log
