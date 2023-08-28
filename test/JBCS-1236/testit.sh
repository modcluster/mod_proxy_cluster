#!/usr/bin/sh

. includes/common.sh

httpd_all_clean
tomcat_all_remove
MPC_NAME=JBCS-1236 httpd_run

# Start a bunch ($1, or 6 if no argument is given) of tomcat
# containers, then test them and stop them
runtomcatbatch() {
    if [ $1 ]; then
      t=$1
    else
      t=5 # default value when no argument is given
    fi

    for i in $(seq $t 10);
    do
      tomcat_start $i 0
    done

    tomcat_count=$(expr 3 + 11 - $t)
    tomcat_wait_for_n_nodes $tomcat_count || exit 1
    for i in $(seq $t 10);
    do
      tomcat_start_webapp $i || exit 1
    done

    # test the tomcats
    sleep 20
    tomcat_all_test_app $tomcat_count
    if [ $? -ne 0 ]; then
      echo "runtomcatbatch tomcat_all_test_app $tomcat_count FAILED!"
      exit 1
    fi

    # "load test" 9 of them
    tomcat_all_run_ab $tomcat_count
    if [ $? -ne 0 ]; then
      echo "runtomcatbatch tomcat_all_run_ab $tomcat_count FAILED!"
      exit 1
    fi

    # retest
    tomcat_all_test_app $tomcat_count
    if [ $? -ne 0 ]; then
      echo "runtomcatbatch tomcat_all_test_app $tomcat_count FAILED!"
      exit 1
    fi

    # stop the tomcats
    for i in $(seq $t 10);
    do
      tomcat_shutdown $i 0
    done

    tomcat_wait_for_n_nodes 3
    if [ $? -ne 0 ]; then
      echo "runtomcatbatch tomcat_wait_for_n_nodes $tomcat_count FAILED!"
      exit 1
    fi

    # remove the tomcats
    for i in $(seq $t 10);
    do
      tomcat_remove $i
    done
    echo "runtomcatbatch Done!"
}

# single tomcat testing
# we start the tomcat, put the webapp, test it and later stop and clean up
singlecycle() {
    echo "singlecycle: Testing tomcat$1"
    R=$1
    if [ "$2" = "useran" ]; then
        R=$(expr 1 + $RANDOM % 10 + 10)
        R=$(expr $R + 2)
        # TODO
        tomcat_start $1 $R || exit 1
    else
        R=0
        tomcat_start $1 $R || exit 1
    fi
    # Wait for it to start
    echo "Testing(0) tomcat$1 waiting..."
    i=0
    while true
    do
        curl -s http://localhost:6666/mod_cluster_manager | grep Node | grep tomcat$1 > /dev/null
        if [ $? -eq 0 ]; then
            break
        fi
        if [ $i -gt 300 ]; then
            echo "Timeout: tomcat$1 is not ready"
            exit 1
        fi
        i=$(expr $i + 1)
        sleep 1
    done
    echo "Testing(0) tomcat$1 started"
    tomcat_start_webapp $1 || exit 1
    echo "Testing(0) tomcat$1 with webapp"
    i=0
    while true
    do
        curl -s http://localhost:6666/mod_cluster_manager | grep /tomcat$1 > /dev/null
        if [ $? -eq 0 ]; then
            break
        fi
        if [ $i -gt 300 ]; then
            echo "Timeout: webapp on tomcat$1 is not ready after 300 seconds"
            exit 1
        fi
        i=$(expr $i + 1)
        sleep 1
    done
    echo "Testing(1) tomcat$1"
    tomcat_test_app $1 || exit 1
    echo "Testing(2) tomcat$1"
    tomcat_test_app $1 || exit 1
    tomcat_run_ab $1 || exit 1
    echo "Testing(3) tomcat$1"
    tomcat_shutdown $1 $R || exit 1
    while true
    do
        curl -s http://localhost:6666/mod_cluster_manager | grep Node | grep tomcat$1 > /dev/null
        if [ $? -ne 0 ]; then
            break
        fi
        if [ $i -gt 300 ]; then
            echo "Timeout: webapp is still present on tomcat$1 after 300 seconds"
            exit 1
        fi
        i=$(expr $i + 1)
        sleep 1
    done
    tomcat_remove $1 || exit 1
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

    sleep ${FOREVER_PAUSE:-3600}

    echo "Doing: kill -15 $pid12 $pid13 $pid14 $pid15 $pid16"
    kill -15 $pid12 $pid13 $pid14 $pid15 $pid16
    if [ $? -ne 0 ]; then
        echo "kill -15 $pid12 $pid13 $pid14 $pid15 $pid16 failed"
        exit 1
    fi
    echo "Tests done, cleaning"
    # stop & remove the containers
    tomcat_remove_by_name tomcat12
    tomcat_remove_by_name tomcat13
    tomcat_remove_by_name tomcat14
    tomcat_remove_by_name tomcat15
    tomcat_remove_by_name tomcat16
    sleep 10
}

# Start and stop successively (one after another) $1 tomcats
cyclestomcats() {
    i=1
    while true
    do
        i=$(expr $i + 1)
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
    tomcat_start 2 0
    tomcat_start 3 0
    tomcat_start 4 0
    tomcat_wait_for_n_nodes 3 || exit 1
    # check them
    tomcat_start_webapp 2 || exit 1
    tomcat_start_webapp 3 || exit 1
    tomcat_start_webapp 4 || exit 1
    sleep 20
    tomcat_test_app 2 || exit 1
    tomcat_test_app 3 || exit 1
    tomcat_test_app 4 || exit 1

    # start a bunch of tomcats, test, shutdown, remove and try in a loop.
    runjbcs1236=0
    while true
    do
        runjbcs1236=$(expr $runjbcs1236 + 1)
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
        tomcat_shutdown 2 0

        tomcat_wait_for_n_nodes 2
        if [ $? -ne 0 ]; then
            echo "tomcat_wait_for_n_nodes 2: runjbcs1236 Failed!"
            exit 1
        fi
        tomcat_remove 2
        tomcat_start 5 0

        tomcat_wait_for_n_nodes 3
        if [ $? -ne 0 ]; then
            echo "tomcat_wait_for_n_nodes 3: runjbcs1236 Failed!"
            exit 1
        fi
        tomcat_start_webapp 5
        if [ $? -ne 0 ]; then
            echo "tomcat_start_webapp 5: runjbcs1236 Failed!"
            exit 1
        fi
        sleep 20
        tomcat_test_app 5
        if [ $? -ne 0 ]; then
            echo "tomcat_test_app 5: runjbcs1236 Failed!"
            exit 1
        fi
        # we have 5 3 4 in shared memory
        # read 2
        tomcat_start 2 0
        tomcat_wait_for_n_nodes 4
        if [ $? -ne 0 ]; then
            echo "tomcat_wait_for_n_nodes 4: runjbcs1236 Failed!"
            exit 1
        fi
        tomcat_start_webapp 2
        if [ $? -ne 0 ]; then
            echo "tomcat_start_webapp 2: runjbcs1236 Failed!"
            exit 1
        fi
        sleep 20
        tomcat_test_app 2
        if [ $? -ne 0 ]; then
            echo "tomcat_test_app 2: runjbcs1236 Failed!"
            exit 1
        fi

        sleep 20

        # we have 5 3 4 2 in shared memory
        # if something was wrong 2 points to 5
        tomcat_shutdown 5 0

        tomcat_wait_for_n_nodes 3
        if [ $? -ne 0 ]; then
            echo "tomcat_wait_for_n_nodes 3: runjbcs1236 Failed!"
            exit 1
        fi
        tomcat_remove 5

        tomcat_test_app 2
        if [ $? -ne 0 ]; then
            echo "tomcat_test_app 2: runjbcs1236 Failed!"
            exit 1
        fi

        tomcat_test_app 3
        if [ $? -ne 0 ]; then
            echo "tomcat_test_app 3: runjbcs1236 Failed!"
            exit 1
        fi

        tomcat_test_app 4
        if [ $? -ne 0 ]; then
            echo "tomcat_test_app 4: runjbcs1236 Failed!"
            exit 1
        fi
        echo "runjbcs1236 loop: $runjbcs1236 DONE"
    done

    # cleanup
    tomcat_shutdown 4 0
    tomcat_shutdown 3 0
    tomcat_shutdown 2 0
    tomcat_wait_for_n_nodes 0 || exit 1
    tomcat_remove 2
    tomcat_remove 3
    tomcat_remove 4
}

# JBCS-1236
echo "Testing JBCS-1236"
cyclestomcats ${TOMCAT_CYCLE_COUNT:-10}
if [ $? -ne 0 ]; then
  echo "JBCS-1236 cyclestomcats 100 FAILED!"
  exit 1
fi
forevertomcat
if [ $? -ne 0 ]; then
  echo "JBCS-1236 forevertomcat FAILED!"
  exit 1
fi
runjbcs1236
if [ $? -ne 0 ]; then
  echo "JBCS-1236 runjbcs1236 FAILED!"
  exit 1
fi

tomcat_all_remove
