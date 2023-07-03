#!/bin/bash

# if ran from main testsuite, change directory
pwd | grep MODCLUSTER-785
if [ $? ]; then
    PREFIX=MODCLUSTER-785
else
    PREFIX="."
fi

. includes/common.sh

# check for a fork or modcluster repository.
if [ -z $SOURCESFORK ]; then
  SOURCESFORK="modcluster"
fi

# check mod_proxy_cluster or mod_cluster
if [ -z $USE13 ]; then
  SOURCES=https://github.com/${SOURCESFORK}/mod_proxy_cluster
  MDBRANCH=main  
else
  SOURCES=https://github.com/${SOURCESFORK}/mod_cluster
  MDBRANCH=1.3.x  
fi

# first stop any previously running tests.
tomcat_all_stop
tomcat_all_remove
httpd_all_clean


# build httpd + mod_proxy_cluster
#
REPOORIGIN=`git remote -v | grep origin | grep fetch | awk ' { print $2 } ' | awk -F/ ' { print $4 } '`
BRANCH=`git branch -v | awk ' { print $2 } '`
MPCCONF="https://raw.githubusercontent.com/${REPOORIGIN}/mod_proxy_cluster/${BRANCH}/test/MODCLUSTER-785/mod_proxy_cluster.conf"

rm -f nohup.out
MPC_SOURCES=${SOURCES} MPC_BRANCH=${MDBRANCH} MPC_CONF=$MPCCONF MPC_NAME=MODCLUSTER-785 httpd_run


# start tomcat1 on 8080
tomcat_start 1

# wait until tomcat1 is in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 1

# copy the test page in app to tomcat8080
docker cp $PREFIX/app tomcat1:/usr/local/tomcat/webapps/app

# check that the app is answering
sleep 15
curl -s http://localhost:8000/app/status.jsp | grep "785"
if [ $? -ne 0 ]; then
  echo "MODCLUSTER-785 Failed!"
  exit 1
fi

# Stop abruptly
tomcat_stop 1
tomcat_remove 1

# it return 503
# make sure we use enough workers
ab -c 10 -n100 http://localhost:8000/app/
http_code=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/app/`
if [ ${http_code} != 503 ]; then
  echo "MODCLUSTER-785 Failed! not 503 but ${http_code}"
  exit 1
fi

sleep 15

# start tomcat1 on 8080
tomcat_start 1

# wait until tomcat1 is in mod_proxy_cluster tables
tomcat_wait_for_n_nodes 1

# copy the test page in app to tomcat8080
docker cp $PREFIX/app tomcat1:/usr/local/tomcat/webapps/app
sleep 15

# check that the app is answering
# does it return 503
http_code=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/app/status.jsp`
i=0
while true
do
  sleep 1
  http_code=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/app/status.jsp`
  if [ ${http_code} == 200 ]; then
    break
  fi
  i=`expr $i + 1`
  if [ $i -gt 60 ]; then
    break
  fi
  echo "*${http_code}*"
done
if [ ${http_code} == 503 ]; then
  echo "MODCLUSTER-785 Failed! return 503"
  exit 1
fi
if [ ${http_code} != 200 ]; then
  echo "MODCLUSTER-785 Failed! not 200 but ${http_code}"
  exit 1
fi

# Redo the test the 503 are only on the worker that hasn't had 60 of ide to recover...
i=0
while true
do
  sleep 1
  http_code=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/app/status.jsp`
  if [ ${http_code} == 503 ]; then
    echo "MODCLUSTER-785 Failed! return 503"
    exit 1
  fi
  i=`expr $i + 1`
  if [ $i -gt 60 ]; then
    break
  fi
  echo "*${http_code}*"
done

# clean tomcats
tomcat_all_remove
# and httpd
httpd_all_clean

echo "MODCLUSTER-785 Done!"
