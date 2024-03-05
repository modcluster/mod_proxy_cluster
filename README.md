mod_cluster
===========

Project mod_cluster is a httpd-based load-balancer. It uses a communication channel to forward
requests from httpd to one of a set of application server nodes. Unlike mod_jk and mod_proxy,
mod_cluster leverages an additional connection between the application server nodes and httpd
to transmit server-side load-balance factors and lifecycle events back to httpd. This additional
feedback channel allows mod_cluster to offer a level of intelligence and granularity not found in
other load-balancing solutions.

Mod_cluster boasts the following advantages over other httpd-based load-balancers:

* Dynamic configuration of httpd workers
* Server-side load balance factor calculation
* Fine grained web-app lifecycle control
* AJP is optional

[https://modcluster.io](https://modcluster.io)


Native modules for httpd
------------------------

Sources for the mod_proxy_cluster module are in the native directory. To build the components from
the sources, you need following tools:

* C compiler
* cmake, or autoconf, automake, and libtool
* make
* httpd (with header files)

For compilation using Linux with cmake, execute following (from the root of this repository):

```sh
mkdir native/build
cd native/build
cmake ..
make
```

The built modules are within `modules` subdirectory.

For more detailed instructions (incl. building on Windows) check out the online documentation.

Styles
------

The codebase follows certain code style that is specified in `.clang-format` file within the `native`
directory. You can run the tool manually from the mentioned directory by executing `clang-format -n <file>`.
To apply all suggestions to a file automatically, run `clang-format -i <file>`.

However, there are some cases where breaking the code style may result in better clarity. In those cases
enclose the corresponding part of code like this

```c
/* clang-format off */
<code>
/* clang-format on */
```

The style check will ignore everything between the two comments. (Please, don't abuse it.)

Tests
-----

The project contains some tests too. You can find them with a separate README in the `test` subdirectory.

Debugging
---------

For the debugging, you can specify the `LogLevel` within you `httpd.conf` file. In case you want detailed logs,
set the level to `DEBUG`. In case that would not be enough, `mod_proxy_cluster` uses messages with level
up to `TRACE6`. Such a level of logging might result in huge and chaotic logs, so we advise you to set these
level for the module only (see [this document](https://httpd.apache.org/docs/2.4/logs.html#permodule)).

You can also get additional information in case you compile mod_proxy_cluster with `HAVE_CLUSTER_EX_DEBUG` macro
set. That will make more information visible within mod_manager. It will also add more information to the context
search, but bear in mind that this may affect the performance.

Documentation
-------------

The documentation can be found on the project's site – [docs.modcluster.io](https://docs.modcluster.io) – where
you can find Getting started guide and documentation for the worker side (Java) part – mod_cluster. 

### Doxygen API docs

The project source files contain doxygen-style comments. To build doxygen doumentation, execute `doxygen` command.
The output can be found in newly created `doxygen-out/` directory. It is
[available online](https://docs.modcluster.io/apidocs/mpc-2.0/) as well.

License
-------
* [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0)
