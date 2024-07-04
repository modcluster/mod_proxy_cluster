# SELinux policies

To compile policies run `make -f /usr/share/selinux/devel/Makefile`

To install policies run `semodule -i mod_cluster.pp`

To remove them run `semodule  -r mod_cluster`

In order to be able to listen and advertise on the default port execute

```bash
semanage port -a -t http_port_t -p udp 23364
semanage port -a -t http_port_t -p tcp 6666
```

To revert the previous run

```bash
semanage port -d -t http_port_t -p udp 23364
semanage port -d -t http_port_t -p tcp 6666
```

For the shared memory tables

```bash
MemManagerFile /var/cache/mod_cluster
mkdir /var/cache/mod_cluster
chcon -v --type=httpd_cache_t /var/cache/mod_cluster
chcon -v --user=system_u /var/cache/mod_cluster
```

Note that the permissions should follow the notice you find in error_log:
```
+++
[Wed Jun 13 09:35:59 2012] [notice] SELinux policy enabled; httpd running as context unconfined_u:system_r:httpd_t:s0
+++
```
