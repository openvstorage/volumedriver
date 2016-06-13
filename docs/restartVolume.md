<a name="restart volume"></a>
## Restart volume
### Open the ipython shell
`ovs`

### Import the storagerouterclient
`import volumedriver.storagerouter.storagerouterclient as src`

### Get the vpool configuration file
#### For Eugene-updates
```
!ps auxf | grep volumedriver_fs | grep -v grep
root      7099  1.0 14.5 17089396 4777244 ?    Ssl  Jun08  73:23 volumedriver_fs -f --config-file=/opt/OpenvStorage/config/storagedriver/storagedriver/vmstor.json
```

#### For Unstable
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
client = src.LocalStorageRouterClient("etcd://127.0.0.1:2379/ovs/vpools/a7d074df-d395-4cf5-af52-dae676642edc/hosts/myvpool-402F26Sp093wQNHvK5/config")
client.restart_pbject("4a0a30de-4914-4abd-be73-e5740e3f14a9", False)
```