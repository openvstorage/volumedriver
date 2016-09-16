# Volume Driver Python API

The volumedriver python api comes with the volumedriver-server

## how to use

### List volumes
```
root@cmp02:~# ovs
Python 2.7.6 (default, Jun 22 2015, 17:58:13) 
Type "copyright", "credits" or "license" for more information.

IPython 1.2.1 -- An enhanced Interactive Python.
?         -> Introduction and overview of IPython's features.
%quickref -> Quick reference.
help      -> Python's own help system.
object?   -> Details about 'object', use 'object??' for extra details.

In [1]: from volumedriver.storagerouter.storagerouterclient import LocalStorageRouterClient as LSRClient
In [2]: client = LSRClient('/opt/OpenvStorage/config/storagedriver/storagedriver/<VPOOL>.json')

In [3]: client.list_volumes()
Out[3]: 
['183a2397-453a-4377-9537-53d591cd2a37',
 '26662acc-20c8-4048-a5d9-483b8ebeb3b0',
 'a4392991-4cea-4c84-89f0-2e8180dcfb9a',
 'bdf6cc3b-9816-4024-ac56-c3929611e10a',
 'dafa3036-d47e-4ebf-ba65-0cf46a0a9a1b']
```

### Create a volume
```
In [4]: from volumedriver.storagerouter.storagerouterclient import MDSMetaDataBackendConfig
In [5]: from volumedriver.storagerouter.storagerouterclient import MDSNodeConfig
In [6]: metadata_backend_config=MDSMetaDataBackendConfig([MDSNodeConfig(address='127.0.0.1', port=26300)])
In [7]: vol = client.create_volume('/mydisk01.raw', metadata_backend_config, '100GiB')
In [8]: print vol
Out[8]: '719d5281-f64d-4108-98c9-73fcc2f8eb12'
```

### Create a snapshot
```
In [9]: client.create_snapshot(vol)
Out[9]: 'Mon Dec 2015 14:23:05 17c4223d-3597-4915-8a4b-4d01ce290756'

In [10]: client.create_snapshot(vol)
Out[10]: 'Mon Dec 2015 14:26:14 f0f916de-38df-4e89-9510-aff8e91bd8fe'

In [11]: client.create_snapshot(vol)
Out[11]: 'Mon Dec 2015 14:26:18 9791f0e1-3b81-4540-990c-20350ca5106d'
```

### List snapshots
```
In [12]: client.list_snapshots(vol)
Out[12]: 
['Mon Dec 2015 14:23:05 17c4223d-3597-4915-8a4b-4d01ce290756',
 'Mon Dec 2015 14:26:14 f0f916de-38df-4e89-9510-aff8e91bd8fe',
 'Mon Dec 2015 14:26:18 9791f0e1-3b81-4540-990c-20350ca5106d']
```
### Delete a snapshot
```
In [13]: snapshots = client.list_snapshots(vol)
In [14]: client.delete_snapshot(vol, snapshots[1])
```


...
