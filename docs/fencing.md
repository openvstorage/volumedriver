## Fencing
One of the key concepts of the Volume Driver is that within a cluster there can only be a single owner of a volume. 
To make sure that once a volume is owned by a new Volume Driver, the old Volume Driver can't access the volume anymore it uses a concept named fencing.
With fencing writes/overwrites to the backend are only permitted if a given condition (e.g. the hash of another object in the same namespace) is met, otherwise the write is refused.
This "conditional write" concept is also extended to the MDS. With these two mechanisms in place, volume ownership is enforced at the backend and MDS level. 
A volumedriver instance can claim ownership of a volume's backend namespace and its MDS, preventing not-quite-dead previous owners (Volume Drivers) from modifying these afterwards.