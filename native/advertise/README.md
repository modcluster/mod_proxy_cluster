# Using mod_advertise

The source can be build with an installed version of httpd-2.4.x, please make
sure it's configured with `--enable-advertise` flag.

```bash
sh buildconf
./configure --with-apache=apache_installation_directory
make
```

The `mod_advertise.so` needs to be copied to `apache_installation_directory/modules`

Configuration use something like the following in httpd.conf:

```
LoadModule advertise_module modules/mod_advertise.so
ServerAdvertise on
AdvertiseGroup 232.0.0.2
AdvertiseFrequency 30
```

Note: default port is 23364.
