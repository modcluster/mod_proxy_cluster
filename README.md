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

Of course the make and the patch must be GNU ones. For example on Solaris you need:
* SMCm4 (requires libsigsegv and libgcc34)
* SMCperl
* SMCautoc
* SMCautom
* SMClibt
* SMCmake
* SMCpatch
* SMCpython

All can be downloaded from [http://www.sunfreeware.com/](http://www.sunfreeware.com/).


License
-------

This software is distributed under the terms of the GNU Lesser General Public License (see [lgpl.txt](lgpl.txt)).

