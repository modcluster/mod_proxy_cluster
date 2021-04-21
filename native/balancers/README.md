# extension for mod_balancer
The module requires mod_balancer

# to use it (example)
```
LoadModule cluster_slotmem_module modules/mod_cluster_slotmem.so
LoadModule manager_module modules/mod_manager.so
LoadModule lbmethod_cluster_module modules/mod_lbmethod_cluster.so
<Location "/balancer-manager">
    SetHandler balancer-manager
</Location>

<Proxy "balancer://xqacluster">
   ProxySet growth=10
</Proxy>

<IfModule manager_module>
  Listen 6666
  ManagerBalancerName xqacluster
  EnableWsTunnel
  WSUpgradeHeader "websocket"
  <VirtualHost *:6666>
   <Directory />
       Require ip 127.0.0.1
       Require ip ::1
    </Directory>

    KeepAliveTimeout 300
    MaxKeepAliveRequests 0

    EnableMCPMReceive
    ServerName localhost

    <Location /mod_cluster_manager>
       SetHandler mod_cluster-manager
       Require ip 127.0.0.1
       Require ip ::1
    </Location>
  </VirtualHost>
</IfModule>
