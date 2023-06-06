#!/bin/bash

# configuration of variables
# if you want tests to pass much faster, decrease these values
if [ -z ${FOREVER_PAUSE+x} ]; then
    FOREVER_PAUSE=3600 # sleep period length during which tomcats are run & stopped
fi
if [ -z ${TOMCAT_CYCLE_COUNT+x} ]; then
    TOMCAT_CYCLE_COUNT=100 # the number of repetitions of a test cycle
fi
if [ -z ${ITERATION_COUNT+x} ]; then
    ITERATION_COUNT=50 # the number of iteration of starting/stopping a tomcat
fi

echo "Test values are FOREVER_PAUSE=$FOREVER_PAUSE, TOMCAT_CYCLE_COUNT=$TOMCAT_CYCLE_COUNT, ITERATION_COUNT=$ITERATION_COUNT"

#
# Stop running given dockered tomcat
stoptomcat() {
    docker container list -a | grep $1
    if [ $? -eq 0 ]; then
        echo "Stopping $1"
        docker stop $1
        if [ $? -ne 0 ]; then
            echo "Can't stop $1"
            exit 1
        fi
    fi
}

#
# Stop running all dockered tomcats
stoptomcats() {
    for i in `docker container list -a --format "{{.Names}}" | grep tomcat`
    do
        stoptomcat $i
    done
}

#
# Wait until there are $1 nodes (i.e., some will start or go away if the count is different)
waitnodes() {
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
        NBNODES=`curl -s http://localhost:6666/mod_cluster_manager | grep "Status: OK" | awk ' { print $3} ' | wc -l`
        sleep 10
        echo "Waiting for ${nodes} node to be ready: `date`"
        i=`expr $i + 1`
        if [ $i -gt 120 ]; then
            echo "Timeout the node(s) number is NOT ${nodes} but ${NBNODES}"
            exit 1
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
# Stop and remove tomcat docker container of a given name
removetomcatname() {
    docker container list -a | grep $1
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

#
# Remove all tomcat containers and images
removetomcats() {
    for i in `docker container list -a --format "{{.Names}}" | grep tomcat`
    do
        removetomcatname $i
    done
}

#
# Start them again
starttwotomcats() {
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

#
# Echo test label message in order to know where we are
echotestlabel() {
    MESS=$1
    echo "***************************************************************"
    echo "Doing test: $MESS"
    echo "***************************************************************"
}

# This should suspend the tomcat for ~ 1000 seconds ~ causing it gets removed afterwhile.
jdbsuspend() {
    rm -f /tmp/testpipein
    mkfifo /tmp/testpipein
    rm -f /tmp/testpipeout
    mkfifo /tmp/testpipeout
    sleep 1000 > /tmp/testpipein &
    podman exec -it tomcat8080 jdb -attach 6660 < /tmp/testpipein > /tmp/testpipeout &
    echo "suspend" > /tmp/testpipein
    cat < /tmp/testpipeout &
}

jdbexit() {
    cat > /tmp/testpipeout &
    echo "exit" > /tmp/testpipein
}

# Start tomcat$1 container on 127.0.0.$1
starttomcat() {
    ADDR="127.0.0.$1"
    if [ $2 -ne 0 ];then
        echo "Starting tomcat$1 on 127.0.0.$2"
        ADDR="127.0.0.$2"
    fi
    echo "Doing: docker run --network=host -e tomcat_ajp_port=8010 -e tomcat_address=$ADDR -e tomcat_shutdown_port=8005 -e jvm_route=tomcat$1 --name tomcat$1 ${IMG}"
    nohup docker run --network=host -e tomcat_ajp_port=8010 -e tomcat_address=$ADDR -e tomcat_shutdown_port=8005 -e jvm_route=tomcat$1 --name tomcat$1 ${IMG} &
    ps -q $! > /dev/null
    if [[ $? -ne 0 ]]; then
	    echo "docker run failed"
	    exit 1
    fi
}

# Start the webapp on the given tomcat
# wait for the tomcat to start.
startwebapptomcat() {
    while true
    do
        docker container list --format "{{.Names}}" | grep tomcat$1
        if [ $? -eq 0 ]; then
            break
        fi
        sleep 2
    done
    docker cp testapp tomcat$1:/usr/local/tomcat/webapps/tomcat$1 || exit 1
}

# Send a shutdown packet to a tomcat$1 container
shutdowntomcat() {
    ADDR="127.0.0.$1"
    if [ $2 -ne 0 ];then
        ADDR="127.0.0.$2"
    fi

    echo "shutdowntomcat at $ADDR"
    echo "SHUTDOWN" | nc $ADDR 8005
}

# Remove the docker image tomcat$1
# Note: To succesfully remove an image it needs to be stopped
removetomcat() {
    docker rm tomcat$1
}

# Test whether the webapp is working (responding)
testtomcat() {
    CODE=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/tomcat$1/test.jsp`
    if [ ${CODE} != "200" ]; then
        echo "Failed can't reach $tomcat$1: ${CODE}"
        exit 1
    fi
}

#
# Run testtomcat for tomcat containers [2..$1]
testtomcats() {
    tc=2
    while true
    do
        testtomcat $tc || exit 1
        tc=`expr $tc + 1`
        if [ $tc -gt $1 ]; then
            echo "testtomcats $tc Done!"
            break
        fi
    done
}

#
# Run a load test for the given tomcat$1 using ab
abtomcat() {
    ab -c10 -n10 http://localhost:8000/tomcat$1/test.jsp > /dev/null
    if [ $? -ne 0 ]; then
        echo "abtomcat: Loading tomcat$1 failed"
        exit 1
    fi
}

#
# Run abtomcat for tomcat containers [2..$1]
abtomcats() {
    tc=2
    while true
    do
        abtomcat $tc || exit 1
        tc=`expr $tc + 1`
        if [ $tc -gt $1 ]; then
            echo "abtomcats: Done!"
            break
        fi
    done
}

#
# Start a bunch ($1, or 6 if no argument is given) of tomcat
# containers, then test them and stop them
runtomcatbatch() {
    if [ $1 ]; then
        t=$1
    else
        t=5 # default value when no argument is given
    fi
    tomcatcount=$t
    # TODO: Change those whiles with arbitrary break to for cycles
    while true
    do
        starttomcat $t 0
        t=`expr $t + 1`
        if [ $t -gt 10 ]; then
            break
        fi
    done
    waitnodes 9  || exit 1
    t=$tomcatcount
    while true
    do
        startwebapptomcat $t || exit 1
        t=`expr $t + 1`
        if [ $t -gt 10 ]; then
            break
        fi
    done

    # test the tomcats
    sleep 20
    testtomcats 9
    if [ $? -ne 0 ];then
        echo "runtomcatbatch testtomcats 9 FAILED!"
        exit 1
    fi

    # "load test" 9 of them
    abtomcats 9
    if [ $? -ne 0 ];then
        echo "runtomcatbatch abtomcats 9 FAILED!"
        exit 1
    fi

    # retest
    testtomcats 9
    if [ $? -ne 0 ];then
        echo "runtomcatbatch testtomcats 9 FAILED!"
        exit 1
    fi

    # stop the tomcats
    t=$tomcatcount
    while true
    do
        shutdowntomcat $t 0
        t=`expr $t + 1`
        if [ $t -gt 10 ]; then
            break
        fi
    done

    waitnodes 3
    if [ $? -ne 0 ];then
        echo "runtomcatbatch waitnodes 3 FAILED!"
        exit 1
    fi

    # remove the tomcats
    t=5
    while true
    do
        removetomcat $t
        t=`expr $t + 1`
        if [ $t -gt 10 ]; then
            break
        fi
    done
    echo "runtomcatbatch Done!"
}

# single tomcat testing
# we start the tomcat, put the webapp, test it and later stop and clean up
singlecycle() {
    echo "singlecycle: Testing tomcat$1"
    R=$1
    if [ "X$2" = "Xuseran" ]; then
        R=`echo $((1 + $RANDOM % 10))`
        R=`expr $R + 2`
        starttomcat $1 $R || exit 1
    else
        R=0
        starttomcat $1 $R || exit 1
    fi
    # Wait for it to start
    echo "Testing(0) tomcat$1 waiting..."
    while true
    do
        curl -s http://localhost:6666/mod_cluster_manager | grep Node | grep tomcat$1 > /dev/null
        if [ $? -eq 0 ]; then
            break
        fi
        sleep 1
    done
    echo "Testing(0) tomcat$1 started"
    startwebapptomcat $1 || exit 1
    echo "Testing(0) tomcat$1 with webapp"
    while true
    do
        curl -s http://localhost:6666/mod_cluster_manager | grep /tomcat$1 > /dev/null
        if [ $? -eq 0 ]; then
            break
        fi
        curl -s http://localhost:6666/mod_cluster_manager | grep /tomcat$1
        sleep 1
    done
    echo "Testing(1) tomcat$1"
    testtomcat $1 || exit 1
    echo "Testing(2) tomcat$1"
    testtomcat $1 || exit 1
    abtomcat $1   || exit 1
    echo "Testing(3) tomcat$1"
    shutdowntomcat $1 $R || exit 1
    while true
    do
        curl -s http://localhost:6666/mod_cluster_manager | grep Node | grep tomcat$1 > /dev/null
        if [ $? -ne 0 ]; then
            break
        fi
        sleep 1
    done
    removetomcat $1 || exit 1
    echo "singlecycle Done tomcat$1"
}

# Run neverending testing loop of a single tomcat
looptomcatforever() {
    while true
    do
        singlecycle $1 || exit 1
    done
}

# Start a bunch of looping tomcats and kill them after $FOREVER_PAUSE (default is 3600 seconds)
forevertomcat() {
    (looptomcatforever 12) &
    pid12=$!
    (looptomcatforever 13) &
    pid13=$!
    # wait a little to prevent synchronization
    sleep 5
    (looptomcatforever 14) &
    pid14=$!
    (looptomcatforever 15) &
    pid15=$!
    (looptomcatforever 16) &
    pid16=$!

    sleep $FOREVER_PAUSE

    echo "Doing: kill -15 $pid12 $pid13 $pid14 $pid15 $pid16"
    kill -15 $pid12 $pid13 $pid14 $pid15 $pid16
    if [ $? -ne 0 ]; then
        echo "kill -15 $pid12 $pid13 $pid14 $pid15 $pid16 failed"
        exit 1
    fi
    echo "Tests done, cleaning"
    # stop & remove the containers
    removetomcatname tomcat12
    removetomcatname tomcat13
    removetomcatname tomcat14
    removetomcatname tomcat15
    removetomcatname tomcat16
    sleep 10
}

# Start and stop successively (one after another) $1 tomcats
cyclestomcats() {
    i=1
    while true
    do
        i=`expr $i + 1`
        if [ $i -gt $1 ]; then
            echo "Looks OK, Done!"
            break
        fi
        singlecycle $i useran || exit 1
    done
}

# run test for https://issues.redhat.com/browse/JBCS-1236
# basically start and stop random tomcats...
runjbcs1236() {
    # start 3 tomcats
    starttomcat 2 0
    starttomcat 3 0
    starttomcat 4 0
    waitnodes 3  || exit 1
    # check them
    startwebapptomcat 2 || exit 1
    startwebapptomcat 3 || exit 1
    startwebapptomcat 4 || exit 1
    sleep 15
    testtomcat 2 || exit 1
    testtomcat 3 || exit 1
    testtomcat 4 || exit 1

    # start a bunch of tomcats, test, shutdown, remove and try in a loop.
    runjbcs1236=0
    while true
    do
        runjbcs1236=`expr $runjbcs1236 + 1`
        if [ $runjbcs1236 -gt 2 ]; then
            echo "Looks OK, runjbcs1236 stopping!"
        break
        fi
        # cycle the tomcats
        runtomcatbatch
         if [ $? -ne 0 ]; then
            echo "runtomcatbatch: runjbcs1236 Failed!"
            exit 1
        fi
        shutdowntomcat 2 0
        waitnodes 2
        if [ $? -ne 0 ]; then
            echo "waitnodes 2: runjbcs1236 Failed!"
            exit 1
        fi
        removetomcat 2
        starttomcat 5 0
        waitnodes 3
        if [ $? -ne 0 ]; then
            echo "waitnodes 3: runjbcs1236 Failed!"
            exit 1
        fi
        startwebapptomcat 5
        if [ $? -ne 0 ]; then
            echo "startwebapptomcat 5: runjbcs1236 Failed!"
            exit 1
        fi
        sleep 20
        testtomcat 5
        if [ $? -ne 0 ]; then
            echo "testtomcat 5: runjbcs1236 Failed!"
            exit 1
        fi
        # we have 5 3 4 in shared memory
        # readd 2
        starttomcat 2 0
        waitnodes 4
        if [ $? -ne 0 ]; then
            echo "waitnodes 4: runjbcs1236 Failed!"
            exit 1
        fi
        startwebapptomcat 2
        if [ $? -ne 0 ]; then
            echo "startwebapptomcat 2: runjbcs1236 Failed!"
            exit 1
        fi
        sleep 20
        testtomcat 2
        if [ $? -ne 0 ]; then
            echo "testtomcat 2: runjbcs1236 Failed!"
            exit 1
        fi
        # we have 5 3 4 2 in shared memory
        # if something was wrong 2 points to 5
        shutdowntomcat 5 0
        waitnodes 3
        if [ $? -ne 0 ]; then
            echo "waitnodes 3: runjbcs1236 Failed!"
            exit 1
        fi
        removetomcat 5
        sleep 20
        testtomcat 2 || exit 1
        if [ $? -ne 0 ]; then
            echo "testtomcat 2: runjbcs1236 Failed!"
            exit 1
        fi
        sleep 20
        testtomcat 3 || exit 1
        if [ $? -ne 0 ]; then
            echo "testtomcat 3: runjbcs1236 Failed!"
            exit 1
        fi
        sleep 20
        testtomcat 4 || exit 1
        if [ $? -ne 0 ]; then
            echo "testtomcat 4: runjbcs1236 Failed!"
            exit 1
        fi
        echotestlabel "runjbcs1236 loop: $runjbcs1236"
    done

    # cleanup
    shutdowntomcat 4 0
    shutdowntomcat 3 0
    shutdowntomcat 2 0
    waitnodes 0  || exit 1
    removetomcat 2
    removetomcat 3
    removetomcat 4
}

#
# Main piece -- tests start here
echotestlabel "Starting tests!!!"
if [ -z ${IMG} ]; then
  echo "IMG needs to defined, please try"
  echo "export IMG=quay.io/${USER}/tomcat_mod_cluster"
  exit 1
fi

# Create files we need
cat << EOF > continue.txt
cont
exit
EOF
cat << EOF > hang.txt
suspend all
exit
EOF

# Clean up possible existing containers before starting...
stoptomcats
waitnodes 0  || exit 1
removetomcats

# JBCS-1236
echotestlabel "JBCS-1236"
cyclestomcats $TOMCAT_CYCLE_COUNT
if [ $? -ne 0 ]; then
  echotestlabel "JBCS-1236 cyclestomcats 100 FAILED!"
  exit 1
fi
forevertomcat
if [ $? -ne 0 ]; then
  echotestlabel "JBCS-1236 forevertomcat FAILED!"
  exit 1
fi
runjbcs1236
if [ $? -ne 0 ]; then
  echotestlabel "JBCS-1236 runjbcs1236 FAILED!"
  exit 1
fi

# Start 2 tomcats, on 8080 and 8081
starttwotomcats || exit 1
waitnodes 2  || exit 1

# Copy testapp and wait for its start
docker cp testapp tomcat8081:/usr/local/tomcat/webapps
sleep 10

# Basic 200 and 404 tests.
echotestlabel "basic 200 and 404 tests"
CODE=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/test.jsp`
if [ ${CODE} != "200" ]; then
  echo "Failed can't reach webapp: ${CODE}"
  exit 1
fi
CODE=`curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/toto.jsp`
if [ ${CODE} != "404" ]; then
  echo "Failed should get 404"
  exit 1
fi

# Sticky (yes, there is only one app!!!)
echotestlabel "sticky one app"
SESSIONCO=`curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
if [ "${SESSIONCO}" == "" ];then
  echo "Failed no sessionid in curl output..."
  curl -v http://localhost:8000/testapp/test.jsp
fi
echo ${SESSIONCO}
NEWCO=`curl -v --cookie "${SESSIONCO}" http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
if [ "${NEWCO}" != "" ]; then
  echo "Failed not sticky received : ${NEWCO}???"
  exit 1
fi

# Copy testapp and wait for starting
docker cp testapp tomcat8080:/usr/local/tomcat/webapps
sleep 10

# Sticky (yes there are 2 apps now)
echotestlabel "sticky 2 app"
SESSIONCO=`curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
NODE=`echo ${SESSIONCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }'`
echo "first: ${SESSIONCO} node: ${NODE}"
NEWCO=`curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
NEWNODE=`echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }'`
echo "second: ${NEWCO} node: ${NEWNODE}"
echo "Checking we can reach the 2 nodes"
i=0
while [ "${NODE}" == "${NEWNODE}" ]
do
  NEWCO=`curl -v http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
  NEWNODE=`echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }'`
  i=`expr $i + 1`
  if [ $i -gt 40 ]; then
    echo "Can't find the 2 webapps"
    exit 1
  fi
  if [ "${NEWNODE}" == "" ]; then
    echo "Can't find node in request"
    exit 1
  fi
  echo "trying other webapp try: ${i}"
  sleep 1
done
echo "${i} try gives: ${NEWCO} node: ${NEWNODE}"

# Still sticky
CO=`curl -v --cookie "${SESSIONCO}" http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
if [ "${CO}" != "" ]; then
  echo "Failed not sticky received : ${CO}???"
  exit 1
fi
CO=`curl -v --cookie "${NEWCO}" http://localhost:8000/testapp/test.jsp -o /dev/null 2>&1 | grep Set-Cookie | awk '{ print $3 } ' | sed 's:;::'`
if [ "${CO}" != "" ]; then
  echo "Failed not sticky received : ${CO}???"
  exit 1
fi

# Stop one of the while running requests.
echotestlabel "sticky: stopping one node and doing requests..."
NODE=`echo ${NEWCO} | awk -F = '{ print $2 }' | awk -F . '{ print $2 }'`
echo $NODE
PORT=`curl http://localhost:6666/mod_cluster_manager | grep Node | grep $NODE | sed 's:)::' | awk -F : '{ print $3 } '`
echo "Will stop ${PORT} corresponding to ${NODE} and cookie: ${NEWCO}"
CODE="200"
i=0
while [ "$CODE" == "200" ]
do
  if [ $i -gt 100 ]; then
    echo "Done remaining tomcat still answering!"
    break
  fi
  CODE=`curl -s -o /dev/null -w "%{http_code}" --cookie "${NEWCO}" http://localhost:8000/testapp/test.jsp`
  if [ $i -eq 0 ]; then
    # stop the tomcat
    echo "tomcat${PORT} being stopped"
    docker stop tomcat${PORT}
    docker container rm tomcat${PORT}
  fi
  i=`expr $i + 1`
done
if [ ${CODE} != "200" ]; then
  echo "Something was wrong... got: ${CODE}"
  curl -v --cookie "${NEWCO}" http://localhost:8000/testapp/test.jsp
  exit 1
fi

# Restart the tomcat
nohup docker run --network=host -e tomcat_port=${PORT} -e tomcat_shutdown_port=true --name tomcat${PORT} ${IMG} &

# Now try to test the websocket
echotestlabel "testing websocket"
# The websocket-hello app is at: https://github.com/jfclere/httpd_websocket
mvn dependency:copy -U -Dartifact=org.apache.tomcat:websocket:hello:0.0.1:war  -DoutputDirectory=. 
if [ $? -ne 0 ]; then
  echo "Something was wrong... can't find org.apache.tomcat:websocket:hello:0.0.1:war"
  cp $HOME/.m2/repository/org/apache/tomcat/websocket-hello/0.0.1/websocket-hello-0.0.1.war .
  if [ $? -ne 0 ]; then
    exit 1
  fi
fi
docker cp websocket-hello-0.0.1.war tomcat8080:/usr/local/tomcat/webapps
docker cp websocket-hello-0.0.1.war tomcat8081:/usr/local/tomcat/webapps
# Put the testapp in the  tomcat we restarted.
docker cp testapp tomcat${PORT}:/usr/local/tomcat/webapps
sleep 10
mvn -f pom-groovy.xml install
java -jar target/test-1.0.jar WebSocketsTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with websocket tests"
  exit 1
fi

#
# Test a keepalived connection finds the 2 webapps on each tomcat
echotestlabel "Testing keepalived with 2 webapps on each tomcat"
docker cp testapp tomcat8080:/usr/local/tomcat/webapps/testapp1
docker cp testapp tomcat8081:/usr/local/tomcat/webapps/testapp2
sleep 10
java -jar target/test-1.0.jar HTTPTest
if [ $? -ne 0 ]; then
  echo "Something was wrong... with HTTP tests"
  exit 1
fi

#
# Test virtual host
echotestlabel "Testing virtual hosts"
docker cp tomcat8081:/usr/local/tomcat/conf/server.xml .
sed '/Host name=.*/i <Host name=\"example.com\"  appBase="examples" />' server.xml > new.xml
docker cp new.xml tomcat8081:/usr/local/tomcat/conf/server.xml
docker cp examples tomcat8081:/usr/local/tomcat
docker commit tomcat8081 quay.io/${USER}/tomcat_mod_cluster-examples
docker stop tomcat8081
docker container rm tomcat8081
waitnodes 1 
# Start the node.
nohup docker run --network=host -e tomcat_port=8081 -e tomcat_shutdown_port=true --name tomcat8081 quay.io/${USER}/tomcat_mod_cluster-examples &
waitnodes 2  || exit 1
# Basically curl --header "Host: example.com" http://127.0.0.1:8000/test/test.jsp gives 200
# in fact the headers are:
# X-Forwarded-For: 127.0.0.1
# X-Forwarded-Host: example.com
# X-Forwarded-Server: fe80::faf4:935b:9dda:2adf
# therefore don't forget ProxyPreserveHost On (otherwise UseAlias On failed...)
#
CODE=`curl -s -o /dev/null -w "%{http_code}" --header "Host: example.com" http://127.0.0.1:8000/test/test.jsp`
if [ ${CODE} != "200" ]; then
  echo "Failed can't rearch webapp at example.com: ${CODE}"
  exit 1
fi
# Basically curl --header "Host: localhost" http://127.0.0.1:8000/test/test.jsp gives 400
CODE=`curl -s -o /dev/null -w "%{http_code}" --header "Host: localhost" http://127.0.0.1:8000/test/test.jsp`
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT rearch webapp at localhost: ${CODE}"
  exit 1
fi
# Same using localhost/testapp2 and curl --header "Host: localhost" http://127.0.0.1:8000/testapp2/test.jsp
CODE=`curl -s -o /dev/null -w "%{http_code}" --header "Host: localhost" http://127.0.0.1:8000/testapp2/test.jsp`
if [ ${CODE} != "200" ]; then
  echo "Failed can't rearch webapp at localhost: ${CODE}"
  exit 1
fi
# Basically curl --header "Host: example.com" http://127.0.0.1:8000/testapp2/test.jsp gives 400
CODE=`curl -s -o /dev/null -w "%{http_code}" --header "Host: example.com" http://127.0.0.1:8000/testapp2/test.jsp`
if [ ${CODE} != "404" ]; then
  echo "Failed should NOT rearch webapp at localhost: ${CODE}"
  exit 1
fi

# Shutdown the 2 tomcats
docker exec -it tomcat8080 /usr/local/tomcat/bin/shutdown.sh
docker exec -it tomcat8081 /usr/local/tomcat/bin/shutdown.sh
waitnodes 0 
docker container rm tomcat8080
docker container rm tomcat8081

# Loop stopping starting the same tomcat
iter=0
while [ $iter -lt $ITERATION_COUNT ]
do
   echo "Loop stopping starting the same tomcat iter: $iter"
   nohup docker run --network=host -e tomcat_port=8080 --name tomcat8080 ${IMG} &
   sleep 10
   waitnodes 1 || exit 1
   docker exec -it tomcat8080 /usr/local/tomcat/bin/shutdown.sh
   waitnodes 0 || exit 1
   docker container rm tomcat8080
   iter=`expr $iter + 1`
done 

# Check that hanging tomcat will be removed
echotestlabel "hanging a tomcat checking it is removed after a while no requests"
PORT=8081
nohup docker run --network=host -e tomcat_port=${PORT} -e tomcat_shutdown_port=true --name tomcat${PORT} ${IMG} &
PORT=8080
nohup docker run --network=host -e tomcat_port=${PORT} -e tomcat_shutdown_port=true --name tomcat${PORT} ${IMG} &
sleep 10
waitnodes 2 || exit 1
# curlloop.sh checks for http://localhost:8000/testapp/test.jsp
docker cp testapp tomcat8080:/usr/local/tomcat/webapps
docker cp testapp tomcat8081:/usr/local/tomcat/webapps
docker cp setenv.sh tomcat${PORT}:/usr/local/tomcat/bin
docker commit tomcat${PORT} quay.io/${USER}/tomcat_mod_cluster-debug
docker stop tomcat${PORT}
waitnodes 1 
docker container rm tomcat${PORT}
# Start the node.
nohup docker run --network=host -e tomcat_port=${PORT} -e tomcat_shutdown_port=true --name tomcat${PORT} quay.io/${USER}/tomcat_mod_cluster-debug &
sleep 10
docker exec tomcat${PORT} jdb -attach 6660 < continue.txt
waitnodes 2  || exit 1
echo "2 tomcat started"
# Hang the node,
# jdb and a pipe to hang the tomcat.
jdbsuspend
waitnodes 1  || exit 1
echo "1 tomcat hanging and gone"
jdbexit
# The tomcat is comming up again
waitnodes 2  || exit 1
echo "the tomcat is back"

# Same test with requests, make them in a loop
echotestlabel "hanging tomcat removed after a while with requests"
bash curlloop.sh 200 000 &
jdbsuspend
waitnodes 1  || exit 1
ps -ef | grep curlloop | grep -v grep
if [ $? -ne 0 ]; then
  echo "curlloop.sh FAILED!"
  exit 1
fi
ps -ef | grep curlloop | grep -v grep | awk ' { print $2 } ' | xargs kill
jdbexit
# The tomcat is comming up again
waitnodes 2  || exit 1

# Same test with requets but stop the other tomcat
echotestlabel "single hanging tomcat removed after a while with requests"
PORT=8081
docker stop tomcat${PORT}
docker container rm tomcat${PORT}
waitnodes 1  || exit 1
jdbsuspend
sleep 10
bash curlloop.sh 000 404 503 &
waitnodes 0  || exit 1
ps -ef | grep curlloop | grep -v grep
if [ $? -ne 0 ]; then
  echo "curlloop.sh FAILED!"
  exit 1
fi
ps -ef | grep curlloop | grep -v grep | awk ' { print $2 } ' | xargs kill
jdbexit
# The tomcat is comming up again
waitnodes 1  || exit 1

# Cleanup at the end
stoptomcats
waitnodes 0  || exit 1
removetomcats
echotestlabel "Done with the tests!!!"
echo "Done!"

