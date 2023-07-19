#!/bin/bash

#
# first stop running tomcats
stoptomcat() {
docker ps -a | grep $1
if [ $? -eq 0 ]; then
  echo "Stopping $1"
  docker stop $1
    if [ $? -ne 0 ]; then
      echo "Can't stop $1"
      exit 1
  fi
fi
}
stoptomcats () {
for i in `docker ps -a --format "{{.Names}}" | grep tomcat`
do
  stoptomcat $i
done
}

#
# Wait the nodes to go away or start
waitnodes () {
nodes=$1
curl -s http://localhost:6666/mod_cluster_manager -o /dev/null
if [ $? -ne 0 ]; then
  echo "httpd no started or something VERY wrong"
  exit 1
fi
NBNODES=-1
i=0
while [ ${NBNODES} != ${nodes} ]
do
  NBNODES=`curl -s http://localhost:6666/mod_cluster_manager | grep Node | awk ' { print $3} ' | wc -l`
  sleep 10
  echo "Waiting for ${nodes} node to be ready: `date`"
  i=`expr $i + 1`
  if [ $i -gt 120 ]; then
    echo "Timeout the node(s) number is NOT ${nodes} but ${NBNODES}"
    exit 1
  fi
  # check if the nodes are OK
  if [ ${NBNODES} = ${nodes} ]; then
    NBNODESOK=`curl -s http://localhost:6666/mod_cluster_manager | grep "Status: OK" | wc -l`
    if [ $NBNODESOK != ${nodes} ]; then
      echo "Some nodes are not in OK state..."
      exit 1
    fi
  fi
done
curl -s http://localhost:6666/mod_cluster_manager -o /dev/null
if [ $? -ne 0 ]; then
  echo "httpd no started or something VERY wrong"
  exit 1
fi
echo "Waiting for the node DONE: `date`"
}

#
# remove them
removetomcatname () {
docker ps -a | grep $1
if [ $? -eq 0 ]; then
  echo "Stopping $1"
  docker stop $1
    if [ $? -ne 0 ]; then
      echo "Can't stop $1"
  fi
  echo "Removing $1"
  docker rm $1
    if [ $? -ne 0 ]; then
      echo "Can't remove $1"
  fi
fi
}
removetomcats () {
for i in `docker ps -a --format "{{.Names}}" | grep tomcat`
do
  removetomcatname $i
done
}

#
# Start them again
starttomcats() {
echo "Starting tomcat8080..."
nohup docker run --network=host -e tomcat_port=8080 -e tomcat_shutdown_port=true --name tomcat8080 ${IMG} &
if [ $? -ne 0 ]; then
  echo "Can't start tomcat8080"
  exit 1
fi
sleep 10
echo "Starting tomcat8081..."
nohup docker run --network=host -e tomcat_port=8081 -e tomcat_shutdown_port=true --name tomcat8081 ${IMG} &
if [ $? -ne 0 ]; then
  echo "Can't start tomcat8081"
  exit 1
fi
echo "2 Tomcats started..."
}

# wait until httpd is started
waitforhttpd () {
  while true
  do
    sleep 10
    grep "cannot open" nohup.out | grep error_log
    if [ $? -eq 0 ]; then
      echo "httpd start failed"
      exit 1
    fi

    grep "resuming normal operations" nohup.out
    if [ $? -eq 0 ]; then
      break
    fi
  done

  # httpd started
  curl -v http://localhost:8000/
  if [ $? -ne 0 ]; then
    echo "Httpd not running???"
    exit 1
  fi
}

# Start tomcat$1 container on 127.0.0.$1
starttomcat() {
    ADDR="127.0.0.$1"
    AJPPORT="8010"
    HTTPPORT=""
    if [ "x$2" != "x" ]; then
      if [ $2 -ne 0 ];then
         echo "Starting tomcat$1 on 127.0.0.$2"
          ADDR="127.0.0.$2"
      fi
    fi
    if [ "x$3" != "x" ]; then
      if [ $3 -ne 0 ];then
         echo "Starting tomcat$1 on 127.0.0.$2 with http on port $3"
         AJPPORT=""
         HTTPPORT="$3"
      fi
    fi
    echo "Doing: docker run --network=host -e tomcat_ajp_port=${AJPPORT} -e tomcat_port=${HTTPPORT} -e tomcat_address=$ADDR -e tomcat_shutdown_port=8005 -e jvm_route=tomcat$1 --name tomcat$1 ${IMG}"
    nohup docker run --network=host -e tomcat_ajp_port=${AJPPORT} -e tomcat_port=${HTTPPORT} -e tomcat_address=$ADDR -e tomcat_shutdown_port=8005 -e jvm_route=tomcat$1 --name tomcat$1 ${IMG} &
    ps -q $! > /dev/null
    if [[ $? -ne 0 ]]; then
            echo "docker run failed"
            exit 1
    fi
}

