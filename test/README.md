# Running tests

There are several tests within the repository. Some common tests are within the root directory, more specialized tests
live within their own subdirectories. All tests can be run by a single shell script `testsuite.sh`. The script itself
is a bit parametrized so that you can influence the duration of testing. Upon invocation, the script outputs values of
its parameters. You can change the value by passing the environment variable of the same name with a desired value.

```sh
# to execute the whole testsuite
sh testsuite.sh
# to execute the testsuite with DEBUG on and only one Tomcat cycle, execute
DEBUG=1 TOMCAT_CYCLE_COUNT=1 sh testsuite.sh
```

You can also run the individual testsuites by yourself, just execute the corresponsing script.

```sh
sh basetests.sh
# alternatively, you can use the run_test function
source includes/common.sh           # load definitions
run_test basetests.sh "Basic tests" # run tests
```

You might find useful `includes` directory, especially `common.sh` script that contains shared functions for the tests
(mainly for controlling container images, see the section below).

You should be able to check tests results based on the `$?` as usual so that you can use it in your scripts and automations.

## Test images

If you use the main `testsuite.sh` script, you don't have to worry about this too much. Just make sure that you have checked
out and built [mod_cluster](https://github.com/modcluster/mod_cluster) in sub directory at the same level you checked out this
repository.

Also, if you use `podman` instead of `docker`, make sure you have `podman-docker` package installed (tests are using `docker`).

If you don't want to use quay.io (the default), just set `IMG` and `HTTPD_IMG` variables to docker, local repository or some
other service.

There are a few helper functions for both images, however, if you want to control the images manually, use the corresponding
Dockerfiles. See the testsuite, mainly `includes/common.sh`, to see how the images are run.

### httpd_mod_cluster

There are two types of images we use in tests. The first one is the httpd image with mod_proxy_cluster. Its Dockerfile can
be found in `httpd` subdirectory and you can create it (as tests do) with the `httpd_create` function. All functions with
the prefix `httpd_` are using this image.

### tomcat_mod_cluster

The second image is a Tomcat image with mod_cluster enabled. Its Dockerfile is in the `tomcat` directory and tests use
`tomcat_create` function. As for httpd, all functions orking with tomcat image are prefixed `tomcat_`.

## Dependencies

There are several dependecies / other repositories you have to have checked out and built/installed, namely:

* https://github.com/jfclere/httpd_websocket
* https://github.com/modcluster/mod_cluster

Alternatively, you can use `setup-dependencies.sh` script that prepares everything for you.

These tools are required for running tests:

* git
* docker (or podman with podman-docker package)
* maven
* curl
* jdb (java)
* ss
* ab

# Testing with miniserver
There is also a python script that can be run to check mod_proxy_cluster. You can find it within `includes` directory
as `miniserver.py`. Execute it simply as `./includes/miniserver.py <port-number>`.

You must have `httpd` with `mod_proxy_cluster` running.
