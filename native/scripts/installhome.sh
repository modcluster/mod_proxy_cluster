#!/bin/sh

#
# Copyright The mod_cluster Project Authors
# SPDX-License-Identifier: Apache-2.0
#

# Arrange mod_cluster installation to be able to run it in $HOME instead /opt
# @author Jean-Frederic Clere

echo ""
echo "Running : `basename $0` $LastChangedDate: 2010-05-04 18:26:40 +0200 (Tue, 04 May 2010) $"
echo ""
# process help
while [ "x" != "x$1" ]
do
  case  $1 in
    --help)
       echo "mod_cluster installer"
       exit 0
       ;;
  esac
done

# find RPM_BUILD_ROOT
RPM_BUILD_ROOT=`dirname $0`
BASE_NAME=`basename $0`
if [ "x${RPM_BUILD_ROOT}" = "x." ]; then
  # local file
  if [ -x $RPM_BUILD_ROOT/$BASE_NAME ]; then
    $0 --help 2>/dev/null | grep "mod_cluster installer" 1>/dev/null
    if [ $? -eq 0 ]; then
      RPM_BUILD_ROOT=`(cd $RPM_BUILD_ROOT/..; pwd)`
    fi
  fi
else
  RPM_BUILD_ROOT=`(cd $RPM_BUILD_ROOT/..; pwd)`
fi
if [ "x${RPM_BUILD_ROOT}" = "x." ]; then
  # in $PATH)
  for path in `echo $PATH | sed "s/:/ /g"`
  do
    file=$path/$0
    $path/$0 --help 2>/dev/null | grep "mod_cluster installer" 1>/dev/null
    if [ $? -eq 0 ]; then
      RPM_BUILD_ROOT=`(cd $path/..; pwd)`
      break
    fi
  done
fi
echo "Installing in $RPM_BUILD_ROOT"

# Process httpd configuration files.
BASEHTTPD=/opt/jboss/httpd
HTTPDCONF=httpd/conf
HTTPDSBIN=sbin
HTTPDBIN=bin
HTTPDBUILD=htdocs/build
if [ -f ${RPM_BUILD_ROOT}/${HTTPDSBIN}/apxs ]; then
  files="${HTTPDSBIN}/apachectl ${HTTPDCONF}/httpd.conf ${HTTPDSBIN}/envvars ${HTTPDSBIN}/apxs ${HTTPDBUILD}/config_vars.mk"
else
  files="${HTTPDSBIN}/apachectl ${HTTPDCONF}/httpd.conf ${HTTPDSBIN}/envvars ${HTTPDBIN}/apxs ${HTTPDBUILD}/config_vars.mk"
fi
for FILE in `echo $files`
do
  file=${RPM_BUILD_ROOT}/$FILE
  echo "$file"
  cp -p $file $file.new
  echo "s:${BASEHTTPD}:${RPM_BUILD_ROOT}:" > sed.cmd
  echo "s/Listen 80.*/Listen 8090/" >> sed.cmd
  sed -f sed.cmd $file > $file.new
  mv $file.new $file
  rm -f sed.cmd
done
# Arrange apachectl
file=$RPM_BUILD_ROOT/${HTTPDSBIN}/apachectl
cp -p $file $file.new
echo "s:\$HTTPD -k \$ARGV:\$HTTPD -k \$ARGV -d $RPM_BUILD_ROOT/httpd:" > sed.cmd
sed -f sed.cmd $file > $file.new
mv $file.new $file
rm -f sed.cmd
