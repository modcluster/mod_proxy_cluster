#!/usr/bin/sh

IMG=${IMG:-mod_proxy_cluster-testsuite-tomcat}
HTTPD_IMG=${HTTPD_IMG:-mod_proxy_cluster-testsuite-httpd}

# Runs a test file ($1) under given name ($2, if given)
run_test() {
    local ret=0
    if [ -z ${2+x} ]; then
        printf "Running %-42s ..." $2
    else
        printf "Running %-42s ..." $1
    fi
    if [ $DEBUG ]; then
        sh $1 > "logs/${2:-$1}.log" 2>&1
    else
        sh $1 > /dev/null 2>&1
    fi
    if [ $? = 0 ]; then
        echo "  OK"
    else
        echo " NOK"
        ret=1
    fi
    # preserve httpd's logs too if DEBUG
    if [ $DEBUG ]; then
        local httpd_cont=$(docker ps -a | grep $HTTPD_IMG | cut -f 1 -d' ')
        docker logs  $httpd_cont > "logs/${2:-$1}-httpd.log" 2>&1
        docker cp ${httpd_cont}:/usr/local/apache2/logs/access_log "logs/${2:-$1}-httpd_access.log" > /dev/null
    fi
    # Clean all after run
    httpd_shutdown > /dev/null 2>&1
    tomcat_all_remove > /dev/null 2>&1
    return $ret
}

#####################################################
### H T T P D   H E L P E R   F U N C T I O N S   ###
#####################################################
# create httpd container
httpd_create() {
    rm -rf httpd/mod_proxy_cluster /tmp/mod_proxy_cluster
    mkdir /tmp/mod_proxy_cluster
    cp -r ../native ../test /tmp/mod_proxy_cluster/
    mv /tmp/mod_proxy_cluster httpd/
    docker build -t $HTTPD_IMG httpd/
}

# Build and run httpd container
httpd_run() {
    # if httpd is already running for some reason, end it
    httpd_shutdown || true
    if [ $DEBUG ]; then
        echo "httpd mod_proxy_cluster image config:"
        echo "    CONF:    ${MPC_CONF:-httpd/mod_proxy_cluster.conf}"
        echo "    NAME:    ${MPC_NAME:-httpd-mod_proxy_cluster}"
        echo "You can config those with envars MPC_SOURCES, MPC_BRANCH, MPC_CONF, MPC_NAME respectively"
    fi
    docker run -d --network=host --name ${MPC_NAME:-httpd-mod_proxy_cluster} \
               -e CONF=${MPC_CONF:-httpd/mod_proxy_cluster.conf} \
               $HTTPD_IMG

    httpd_wait_until_ready
}

httpd_wait_until_ready() {
    local i=0
    curl localhost:6666 > /dev/null 2>&1
    while [ $? != 0 ];
    do
        i=$(expr $i + 1)
        if [ $i -gt 20 ]; then
            echo "Failed to run httpd container"
            exit 1;
        fi
        sleep 10;
        curl localhost:6666 > /dev/null 2>&1
    done
    echo "httpd ready after $i attempts"
}

httpd_shutdown() {
    docker ps --format "{{.Names}}" | grep ${MPC_NAME:-httpd-mod_proxy_cluster}
    if [ $? = 0 ]; then
        docker stop ${MPC_NAME:-httpd-mod_proxy_cluster}
    fi
    docker ps -a --format "{{.Names}}" | grep ${MPC_NAME:-httpd-mod_proxy_cluster}
    if [ $? = 0 ]; then
        docker rm ${MPC_NAME:-httpd-mod_proxy_cluster}
    fi
}

httpd_all_clean() {
    for i in $(docker ps -a | grep "$HTTPD_IMG\|MODCLUSTER\|JBCS" | cut -f1 -d' ');
    do
        docker stop $i
        docker rm $i
    done
}

clean_and_exit() {
    httpd_all_clean
    exit ${1:-1}
}

#####################################################
### T O M C A T   H E L P E R   F U N C T I O N S ###
#####################################################
tomcat_create() {
    docker build -t $IMG tomcat/ --build-arg TESTSUITE_TOMCAT_VERSION=${1:-8.5}
}

# Start tomcat$1 container on 127.0.0.$2
# or 127.0.0.$1 if $2 is not given
# arguments:
#       $1 tomcat number (required)
#       $2 tomcat's last byte of IPv4 address (if 0 or omitted, equals to $1)
#       $3 tomcat port          (if omitted it's 8080 + $1 - 1)
#       $4 tomcat ajp port      (if omitted it's 8900 + $1 - 1)
#       $5 tomcat shutdown port (if omitted it's 8005 + $1 - 1)
tomcat_start() {
    if [ -z "$1" ]; then
        echo "tomcat_start called without arguments"
        exit 1
    fi
    ADDR="127.0.0.$1"
    if [ ${2:-0} -ne 0 ]; then
        ADDR="127.0.0.$2"
    fi

    local portdef=$(expr 8080 + $1 - 1)
    local ajpdef=$(expr 8900 + $1 - 1)
    local shutdef=$(expr 8005 + $1 - 1)

    echo "Starting tomcat$1 on $ADDR"
    nohup docker run --network=host -e tomcat_ajp_port=${4:-$ajpdef} \
                                    -e tomcat_address=$ADDR \
                                    -e tomcat_port=${3:-$portdef} \
                                    -e tomcat_shutdown_port=${5:-$shutdef} \
                                    -e jvm_route=tomcat$1 \
                                --name tomcat$1 ${IMG} &
    ps -q $! > /dev/null
    if [[ $? -ne 0 ]]; then
	    echo "docker run for tomcat$1 failed"
	    exit 1
    fi
}

#
# Stop running given dockered tomcat
tomcat_stop() {
    docker ps | grep tomcat$1
    if [ $? -eq 0 ]; then
        docker stop tomcat$1
        if [ $? -ne 0 ]; then
            echo "Can't stop tomcat$1"
            exit 1
        fi
    else
        echo "$1 is not running"
    fi
}

#
# Stop running all dockered tomcats
tomcat_all_stop() {
    for i in $(docker ps -a --format "{{.Names}}" | grep tomcat | sed -e 's/tomcat//g')
    do
        tomcat_stop $i
    done
}

#
# Wait until there are $1 nodes in OK state (i.e., some will start or go away if the count is different)
tomcat_wait_for_n_nodes() {
    nodes=${1:-0}
    curl -s http://localhost:6666/mod_cluster_manager -o /dev/null
    if [ $? -ne 0 ]; then
        echo "httpd isn't running or something is VERY wrong"
        exit 1
    fi
    NBNODES=-1
    i=0
    while [ ${NBNODES} != ${nodes} ]
    do
        NBNODES=$(curl -s http://localhost:6666/mod_cluster_manager | grep "Status: OK" | awk ' { print $3} ' | wc -l)
        sleep 10
        echo "Waiting for ${nodes} node to be ready: $(date)"
        i=$(expr $i + 1)
        if [ $i -gt 60 ]; then
            echo "Timeout! There are not $nodes nodes but $NBNODES instead"
            exit 1
        fi
    done
    curl -s http://localhost:6666/mod_cluster_manager -o /dev/null
    if [ $? -ne 0 ]; then
        echo "httpd isn't running or something VERY wrong"
        exit 1
    fi
    echo "Waiting for the nodes DONE: $(date)"
}

#
# Stop and remove tomcat docker container of a given name
tomcat_remove_by_name() {
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

#
# Remove all tomcat containers and images
tomcat_all_remove() {
    for i in $(docker ps -a --format "{{.Names}}" | grep tomcat)
    do
        tomcat_remove_by_name $i
    done
}

tomcat_start_two() {
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

# Start the webapp on the given tomcat
# wait for the tomcat to start.
tomcat_start_webapp() {
    while true
    do
        docker ps --format "{{.Names}}" | grep tomcat$1
        if [ $? -eq 0 ]; then
            break
        fi
        sleep 2
    done
    docker cp testapp tomcat$1:/usr/local/tomcat/webapps/tomcat$1 || exit 1
}

# Send a shutdown packet to a tomcat$1 container
# arguments:
#     $1 tomcat number
#     $2 the last segment of IPv4 addr ($1 by default)
#     $3 the shutdown port (8005 + $1 - 1 by default)
tomcat_shutdown() {
    ADDR="127.0.0.$1"
    if [ $2 -ne 0 ]; then
        ADDR="127.0.0.$2"
    fi

    echo "shutting down tomcat$1 with address: $ADDR"
    echo "SHUTDOWN" | nc $ADDR ${3:-$(expr 8005 + $1 - 1)}
}

# Remove the docker image tomcat$1
# Note: To succesfully remove an image it needs to be stopped
tomcat_remove() {
    docker stop tomcat$1 > /dev/null 2>&1
    docker rm tomcat$1
}

#
# Run a load test for the given tomcat$1 using ab
tomcat_run_ab() {
    ab -c10 -n10 http://localhost:8000/tomcat$1/test.jsp > /dev/null
    if [ $? -ne 0 ]; then
        echo "abtomcat: Loading tomcat$1 failed"
        exit 1
    fi
}

#
# Run abtomcat for tomcat containers [2..$1]
tomcat_all_run_ab() {
    tc=2
    while true
    do
        tomcat_run_ab $tc || exit 1
        tc=$(expr $tc + 1)
        if [ $tc -gt $1 ]; then
            echo "abtomcats: Done!"
            break
        fi
    done
}

# Test whether the webapp is working (responding)
tomcat_test_app() {
    CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8000/tomcat$1/test.jsp)
    if [ ${CODE} != "200" ]; then
        echo "Failed can't reach tomcat$1: ${CODE}"
        exit 1
    fi
}

#
# Run tomcat_test for tomcat containers [2..$1]
tomcat_all_test_app() {
    tc=2
    while true
    do
        tomcat_test_app $tc || exit 1
        tc=$(expr $tc + 1)
        if [ $tc -gt $1 ]; then
            echo "tomcat_tests $tc Done!"
            break
        fi
    done
}
