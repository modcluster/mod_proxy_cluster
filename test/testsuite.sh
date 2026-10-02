#!/usr/bin/sh
# exits with 0 if everything went well
# exits with 1 if some test failed
# exits with 2 if httpd container build failed
# exits with 3 if tomcat container build failed

echo "Test parameters are:"
echo "        FOREVER_PAUSE=${FOREVER_PAUSE:=3600}"          # sleep period length during which tomcats are run & stopped
echo "        TOMCAT_CYCLE_COUNT=${TOMCAT_CYCLE_COUNT:=100}" # the number of repetitions of a test cycle
echo "        ITERATION_COUNT=${ITERATION_COUNT:=50}"        # the number of iteration of starting/stopping a tomcat
echo "        IMG=${IMG:=mod_proxy_cluster-testsuite-tomcat}"            # tomcat container image
echo "        HTTPD_IMG=${HTTPD_IMG:=mod_proxy_cluster-testsuite-httpd}" # httpd with mod_proxy_cluster container image
if [ ! -z ${MPC_CONF+x} ]; then
    echo "        MPC_CONF=$MPC_CONF"
fi
echo "        DEBUG=${DEBUG:-Off (undefined)}"
echo "        MOD_PROXY_CLUSTER_TESTS=${MOD_PROXY_CLUSTER_TESTS:=On}"
echo "        MOD_PROXY_BALANCER_TESTS=${MOD_PROXY_BALANCER_TESTS:=On}"
echo "        SKIP_CONTAINER_CREATION=${SKIP_CONTAINER_CREATION:=Off}"
export FOREVER_PAUSE TOMCAT_CYCLE_COUNT ITERATION_COUNT IMG HTTPD_IMG


if [ ! -d logs ]; then
    mkdir logs
fi

. includes/common.sh

IMG_NOVER=$(echo $IMG | cut -d: -f1)

if is_enabled "$SKIP_CONTAINER_CREATION"; then
    # let's just check all the containers are present
    for tomcat_image in "$IMG_NOVER:latest" "$IMG_NOVER:9.0" "$IMG_NOVER:10.1" "$IMG_NOVER:11.0"
    do
        if ! docker image inspect $tomcat_image > /dev/null 2>&1; then
            echo "tomcat image $tomcat_image is missing"
            exit 3
        fi
    done
    if ! docker image inspect $HTTPD_IMG > /dev/null 2>&1; then
        echo "httpd image $HTTPD_IMG is missing"
        exit 2
    fi
else
    # create all containers
    if [ ! -d tomcat/target ]; then
        echo "Missing dependencies. Please run setup-dependencies.sh and then try again"
        exit 4 
    fi

    echo "Creating docker containers..."
    test_create_all_containers
    echo "Done"
fi

# clean everything at first
echo -n "Cleaning possibly running containers..."
httpd_remove   > /dev/null 2>&1
tomcat_all_remove > /dev/null 2>&1
echo " Done"

res=0

if is_enabled "$MOD_PROXY_CLUSTER_TESTS"; then
    for tomcat_version in "9.0" "10.1" "11.0"
    do
        IMG="$IMG_NOVER:$tomcat_version" run_test basetests.sh "Basic tests with tomcat $tomcat_version"
        res=$(( $res + $? ))
    done
    run_test hangingtests.sh            "Hanging tests"
    res=$(( $res + $? ))
    run_test maintests.sh               "Main tests"
    res=$(( $res + $? ))
    run_test websocket/basic.sh         "Websocket tests"
    res=$(( $res + $? ))
    run_test usealias/testit.sh         "UseAlias"
    res=$(( $res + $? ))
    run_test MODCLUSTER-640/testit.sh   "MODCLUSTER-640"
    res=$(( $res + $? ))
    run_test MODCLUSTER-734/testit.sh   "MODCLUSTER-734"
    res=$(( $res + $? ))
    run_test MODCLUSTER-736/testit.sh   "MODCLUSTER-736"
    res=$(( $res + $? ))
    run_test MODCLUSTER-755/testit.sh   "MODCLUSTER-755"
    res=$(( $res + $? ))
    run_test MODCLUSTER-785/testit.sh   "MODCLUSTER-785"
    res=$(( $res + $? ))
    run_test MODCLUSTER-794/testit.sh   "MODCLUSTER-794"
    res=$(( $res + $? ))
fi

if is_enabled "$MOD_PROXY_BALANCER_TESTS"; then
    for tomcat_version in "9.0" "10.1" "11.0"
    do
        IMG="$IMG_NOVER:$tomcat_version" MPC_CONF=httpd/mod_lbmethod_cluster.conf run_test basetests.sh "Basic tests with mod_proxy_balancer and tomcat $tomcat_version"
        res=$(( $res + $? ))
    done
    MPC_CONF=MODCLUSTER-640/mod_lbmethod_cluster.conf run_test MODCLUSTER-640/testit.sh   "MODCLUSTER-640 with mod_proxy_balancer"
    res=$(( $res + $? ))
    MPC_CONF=MODCLUSTER-734/mod_lbmethod_cluster.conf run_test MODCLUSTER-734/testit.sh   "MODCLUSTER-734 with mod_proxy_balancer"
    res=$(( $res + $? ))
    MPC_CONF=httpd/mod_lbmethod_cluster.conf run_test MODCLUSTER-755/testit.sh            "MODCLUSTER-755 with mod_proxy_balancer"
    res=$(( $res + $? ))
    MPC_CONF=MODCLUSTER-785/mod_lbmethod_cluster.conf run_test MODCLUSTER-785/testit.sh   "MODCLUSTER-785 with mod_proxy_balancer"
    res=$(( $res + $? ))
    MPC_CONF=MODCLUSTER-794/mod_lbmethod_cluster.conf run_test MODCLUSTER-794/testit.sh   "MODCLUSTER-794 with mod_proxy_balancer"
    res=$(( $res + $? ))
fi

echo -n "Cleaning containers if any..."
httpd_remove   > /dev/null 2>&1
tomcat_all_remove > /dev/null 2>&1
echo " Done" 

if [ $res -eq 0 ]; then
    echo "Tests finished successfully!"
else
    echo "Tests finished, but some failed."
    res=1
fi

exit $res
