<a name="restart volume"></a>
# Restart volume
To restart a halted volume, you'll need to get the volume-id from the volume you want to restart and the configuration file from the volumedriver.
When the volume is halted, you'll get the following message:
```
root@ovs03:~# ls /mnt/myvpool/* -alh
ls: cannot access /mnt/myvpool/DISK_2688.raw: No such file or directory
?????????? ? ?   ?      ?            ? DISK_2688.raw
```

## Open the ipython shell
`ovs`

### Get the volume-id from the halted volume
```
from ovs.dal.lists.vdisklist import VDiskList
for disk in VDiskList.get_vdisks():
    if disk.info['vrouter_id'] == '':
        print disk.name
        print "==> {0}".format(disk.volume_id)
        print ""
```

### Import the storagerouterclient
`import volumedriver.storagerouter.storagerouterclient as src`

### Get the vpool configuration file

```
 pgrep -a volumedriver
66484 volumedriver_fs -f --config arakoon://config/ovs/vpools/de499fc9-decd-4e38-83a5-ba43f77f80b4/hosts/myvpool013K6zCllKaoL5gXUY/config?ini=%2Fopt%2FOpenvStorage%2Fconfig%2Farakoon_cacc.ini --lock-file /opt/OpenvStorage/run/storagedriver_myvpool01.lock --logrotation --mountpoint /mnt/myvpool01 --logsink console: -o big_writes -o sync_read -o allow_other -o use_ino -o default_permissions -o uid=1001 -o gid=1001 -o umask=0002
```
 In the above example the config file is `arakoon://config/ovs/vpools/de499fc9-decd-4e38-83a5-ba43f77f80b4/hosts/myvpool013K6zCllKaoL5gXUY/config?ini=%2Fopt%2FOpenvStorage%2Fconfig%2Farakoon_cacc.ini`

### Restart the volume
```
client = src.LocalStorageRouterClient("insert_the_config_file_between_the_double_quotes")
client.restart_object("insert_the_volumeid_between_the_double_quotes", False)
```

### Example
```
ovs
import volumedriver.storagerouter.storagerouterclient as src
client = src.LocalStorageRouterClient("arakoon://config/ovs/vpools/de499fc9-decd-4e38-83a5-ba43f77f80b4/hosts/myvpool013K6zCllKaoL5gXUY/config?ini=%2Fopt%2FOpenvStorage%2Fconfig%2Farakoon_cacc.ini")
client.restart_object("4a0a30de-4914-4abd-be73-e5740e3f14a9", False)
```

