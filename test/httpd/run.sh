#!/bin/sh

pwd
ls -lt

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

