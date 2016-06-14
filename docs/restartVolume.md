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
#### For Eugene-updates
```
!ps auxf | grep volumedriver_fs | grep -v grep
root      7099  1.0 14.5 17089396 4777244 ?    Ssl  Jun08  73:23 volumedriver_fs -f --config-file=/opt/OpenvStorage/config/storagedriver/storagedriver/myvpool.json
```

#### For Fargo and Unstable (ETCD)
```
!ps auxf | grep volumedriver_fs | grep -v grep
root      68391 11.9  4.9 4570448 1603728 ?     Ssl  May20 4141:20 volumedriver_fs -f --config etcd://127.0.0.1:2379/ovs/vpools/9380e4b4-5be5-4b33-bb52-2e053f1ce2f3/hosts/myvpoolu7lSr0ANgbo68o0O/config
```

### Restart the volume
```
client = src.LocalStorageRouterClient("insert_the_config_file_between_the_double_quotes")
client.restart_object("insert_the_volumeid_between_the_double_quotes", False)
```

### Example
```
ovs
import volumedriver.storagerouter.storagerouterclient as src
client = src.LocalStorageRouterClient("etcd://127.0.0.1:2379/ovs/vpools/9380e4b4-5be5-4b33-bb52-2e053f1ce2f3/hosts/myvpoolu7lSr0ANgbo68o0O/config")
client.restart_object("4a0a30de-4914-4abd-be73-e5740e3f14a9", False)
```

