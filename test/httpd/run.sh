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

# podman networking can't do rDNS, so REMOTE_HOST is an IP address instead of
# a hostname meaning the `Require host ...` does not work.
# Because of that, we'll add a `Require ip ...` upon podman detection
if grep -qc 'search dns.podman' /etc/resolv.conf; then
    PODMAN_SUBNET=$(sed -rn 's/nameserver ([0-9]+)\.([0-9]+)\.([0-9]+).*/\1\.\2\.\3\./p' /etc/resolv.conf)
    echo "Podman detected: Changing Require host to Require ip $PODMAN_SUBNET"
    sed -i "s/Require host \.mod_proxy_cluster_testsuite_net/Require ip ${PODMAN_SUBNET}/" /usr/local/apache2/conf/$FILECONF
fi

if [ ! -z "$MPC_NAME" ]; then
    sed -i "s/ServerName httpd-mod_proxy_cluster/ServerName ${MPC_NAME}/g" /usr/local/apache2/conf/$FILECONF
fi

# start apache httpd server in foreground
echo "Starting httpd..."
/usr/local/apache2/bin/apachectl start
tail -f /usr/local/apache2/logs/error_log
