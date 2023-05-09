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


Project Structure
-----------------

```
native (native httpd modules)
```

### Reverse Proxy (httpd) Modules

To build the native component from the sources you need a C compiler and the following tools:
* m4
* perl
* autoconf
* automake
* libtool
* make
* patch
* python

Styles
------

The codebase follows certain code style that is specified in `.clang-format` file within the `native`
directory. You can run the tool manualy from the mentioned directory by executing `clang-format -n <file>`.

However, there are some cases where breaking the code style may result in better clarity. In those cases
enclose the corresponding part of code like this

```
/* clang-format off */
<code>
/* clang-format on */
```

The style check will ignore everything between the two comments. (Please, don't abuse it.)

License
-------

This software is distributed under the terms of the GNU Lesser General Public License (see [lgpl.txt](lgpl.txt)).

