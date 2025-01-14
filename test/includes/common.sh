#!/usr/bin/sh

IMG=${IMG:-mod_proxy_cluster-testsuite-tomcat}
HTTPD_IMG=${HTTPD_IMG:-mod_proxy_cluster-testsuite-httpd}
MPC_NAME=${MPC_NAME:-httpd-mod_proxy_cluster}

if [ $CODE_COVERAGE ]; then
    MPC_CFLAGS="$MPC_CFLAGS --coverage -fprofile-arcs -ftest-coverage -g -O0"
    MPC_LDFLAGS="$MPC_LDFLAGS -lgcov"
    HTTPD_EXTRA_FLAGS="$HTTPD_EXTRA_FLAGS --enable-debugger-mode"
fi

# Runs a test file ($1) under given name ($2, if given)
run_test() {
    local ret=0
    if [ ! -z "$2" ]; then
        printf "Running %-42s ..." "$2"
    else
        printf "Running %-42s ..." "$1"
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

    local httpd_cont=$(docker ps -a | grep $HTTPD_IMG | cut -f 1 -d' ')
    # preserve httpd's logs too if DEBUG
    if [ $DEBUG ]; then
        docker logs ${httpd_cont} > "logs/${2:-$1}-httpd.log" 2>&1
        docker cp ${httpd_cont}:/usr/local/apache2/logs/access_log "logs/${2:-$1}-httpd_access.log" 2> /dev/null || true
    fi

    if [ $CODE_COVERAGE ]; then
        docker exec ${httpd_cont} /usr/local/apache2/bin/apachectl stop
        # preserve the coverage files
        # docker has problems with names containing spaces
        f=$(echo ${2:-1} | sed 's/ /-/g')
        docker exec ${httpd_cont} sh -c "cd /native; gcovr --gcov-ignore-errors=no_working_dir_found --json /coverage/coverage-$f.json > /coverage/coverage-$f.log 2>&1"
        docker exec ${httpd_cont} sh -c "cd /native; lcov --capture --directory . --output-file /coverage/coverage-$f.info"

        for f in $(docker exec ${httpd_cont} ls /coverage/); do
            docker cp ${httpd_cont}:/coverage/$f $PWD/coverage/$f > /dev/null
        done
    fi

    # Clean all after run
    httpd_remove > /dev/null 2>&1
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
    # make sure the native are cleaned before copying
    for m in advertise mod_proxy_cluster balancers mod_manager
    do
        cd ../native/$m
        echo "Cleaning $m"
        if [ -f Makefile ]; then
          make clean
        fi
        cd $OLDPWD
    done
    cp -r ../native ../test /tmp/mod_proxy_cluster/
    mv /tmp/mod_proxy_cluster httpd/

    docker build -t $HTTPD_IMG --build-arg CFLAGS="$MPC_CFLAGS" \
                               --build-arg LDFLAGS="$MPC_LDFLAGS" \
                               --build-arg HTTPD_EXTRA_FLAGS="$HTTPD_EXTRA_FLAGS" \
                 -f httpd/Containerfile httpd/
}

# Build and run httpd container
httpd_start() {
    # if httpd is already running for some reason, end it
    httpd_remove || true
    if [ $DEBUG ]; then
        echo "httpd mod_proxy_cluster image config:"
        echo "    CONF:    ${MPC_CONF:-httpd/mod_proxy_cluster.conf}"
        echo "    NAME:    ${MPC_NAME:-httpd-mod_proxy_cluster}"
        echo "You can config those with envars MPC_SOURCES, MPC_BRANCH, MPC_CONF, MPC_NAME respectively"
    fi
    docker run -d --network=host --ulimit nofile=65536:65536 --name ${MPC_NAME:-httpd-mod_proxy_cluster} \
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

httpd_remove() {
    for i in $(docker ps -a | grep "$HTTPD_IMG\|MODCLUSTER\|JBCS\|${MPC_NAME:-httpd-mod_proxy_cluster}" | cut -f1 -d' ');
    do
        docker stop $i
        docker rm $i
    done
}

clean_and_exit() {
    httpd_remove
    exit ${1:-1}
}

#####################################################
### T O M C A T   H E L P E R   F U N C T I O N S ###
#####################################################
# Function that build a container. IMG variable is used for the container's tag.
# By passing arguments you can change
#       $1 tomcat version      (default is 10.1, see tomcat/Dockerfile)
#       $2 tomcat config file  (default is server.xml)
#       $3 tomcat context file (default is context.xml)
tomcat_create() {
    if [ -z "$1" ]; then
        docker build -t $IMG -f tomcat/Containerfile tomcat/ \
                                     --build-arg TESTSUITE_TOMCAT_CONFIG=${2:-server.xml} \
                                     --build-arg TESTSUITE_TOMCAT_CONTEXT=${3:-context.xml}
    else
        docker build -t $IMG -f tomcat/Containerfile tomcat/ \
                                     --build-arg TESTSUITE_TOMCAT_VERSION=$1 \
                                     --build-arg TESTSUITE_TOMCAT_CONFIG=${2:-server.xml} \
                                     --build-arg TESTSUITE_TOMCAT_CONTEXT=${3:-context.xml}
    fi
}

# Start tomcat$1 container on $2 or 127.0.0.$1 if $2 is not given.
# Ports are set by default as follows
#     * tomcat port           8080 + $1 - 1
#     * tomcat ajp port       8900 + $1 - 1
#     * tomcat shutdown port  8005 + $1 - 1
# $1 has to be in range [1, 75].
# Proxy's IP can be specified by $3 (default: 127.0.0.1) and its
# port with $4 (default: 6666).
tomcat_start() {
    if [ -z "$1" ]; then
        echo "tomcat_start called without arguments"
        exit 1
    fi

    if [ $1 -le 0 ] || [ $1 -gt 75 ]; then
        echo "tomcat_start called with invalid \$1 value (got $1, allowed [1, 75])"
        exit 2
    fi
    ADDR="127.0.0.$1"
    if [ ! -z "$2" ]; then
        ADDR="$2"
    fi

    local OFFSET=$(expr $1 - 1)
    echo "Starting tomcat$1 on $ADDR:$(expr 8080 + $OFFSET)"
    nohup docker run --network=host -e tomcat_address=$ADDR \
                                    -e tomcat_port_offset=$OFFSET \
                                    -e jvm_route=tomcat$1 \
                                    -e cluster_address=${3:-127.0.0.1} \
                                    -e cluster_port=${4:-6666} \
                                --name tomcat$1 ${IMG} &
    ps -q $! > /dev/null
    if [ $? -ne 0 ]; then
	    echo "docker run for tomcat$1 failed"
	    exit 1
    fi
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
    echo "Starting tomcat1..."
    tomcat_start 1
    if [ $? -ne 0 ]; then
        echo "Can't start tomcat1"
        exit 1
    fi
    sleep 10
    echo "Starting tomcat2..."
    tomcat_start 2
    if [ $? -ne 0 ]; then
        echo "Can't start tomcat2"
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
tomcat_shutdown() {
    ADDR="127.0.0.$1"
    if [ ! -z "$2" ]; then
        ADDR=$2
    fi

    echo "shutting down tomcat$1 with address: $ADDR"
    echo "SHUTDOWN" | nc $ADDR $(expr 8005 + $1 - 1)
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
