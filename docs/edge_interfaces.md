<a name="edge interfaces"></a>
## Edge interfaces
There are a couple of block interface bindings we provide for starters:
- [QEMU blockdriver](#qemu-blockdriver)
- [blktap](#blktap)
- [TGT iscsi](#tgt)

Following some documentation on how these can be used.

### QEMU blockdriver
QEMU VM Image on OpenvStorage volume is specified like this:


    file.transport=[shm|tcp|rdma],
    file.driver=openvstorage,file.volume=,
    [file.host=server,file.port=port],
    [file.snapshot-timeout=120]


or

    file=openvstorage:
    file=openvstorage:[:snapshot-timeout=]

or

    file=openvstorage[+transport]:[server[:port]/volume_name]:
    [snapshot-timeout=]


where

- 'openvstorage' is the protocol.
- 'transport' is the transport type used to connect to OpenvStorage.
Valid transport types are *shm*, *tcp* and *rdma*. If a transport type isn't
specified then shm is assumed.
- 'server' specifies the server where the volume resides. This can be either
hostname or ipv4 address. If transport type is shm, then 'server' field
should not be specified.
- 'port' is the port number on which OpenvStorage network interface is
listening. This is optional and if not specified QEMU will use the default
port. If the transport type is shm, then 'port' should not be specified.
- 'volume_name' is the name of the OpenvStorage volume.
- 'snapshot-timeout' is the timeout for the volume snapshot to be synced on
the backend. If the timeout expires then the snapshot operation will fail.

Examples:

    file.driver=openvstorage,file.volume=my_vm_volume
    file.driver=openvstorage,file.volume=my_vm_volume, \
    file.snapshot-timeout=120

or

    file=openvstorage:my_vm_volume
    file=openvstorage:my_vm_volume:snapshot-timeout=120

or

    file=openvstorage+tcp:1.2.3.4:21321/my_volume,snapshot-timeout=120

or

    file.driver=openvstorage,file.transport=rdma,file.volume=my_vm_volume,
    file.snapshot-timeout=120,file.host=1.2.3.4,file.port=21321


#### To easily instruct QEMU to use a Open vStorage volume via the Edge interface we integrate with libvirt
@TODO Include an example on how to create VM with Open vStorage Edge volume using libvirt API.

In libvirt a volume under a domain can be specified like this:


    <domain type='qemu'>
        <name>QEMUGuest1</name>
        <uuid>c7a5fdbd-edaf-9455-926a-d65c16db1809</uuid>
        <memory unit='KiB'>219136</memory>
        <currentMemory unit='KiB'>219136</currentMemory>
        <vcpu placement='static'>1</vcpu>
        <os> <type arch='i686' machine='pc'>hvm</type> <boot dev='hd'/> </os>
        <clock offset='utc'/>
        <on_poweroff>destroy</on_poweroff>
        <on_reboot>restart</on_reboot>
        <on_crash>destroy</on_crash>
        <devices>
            <emulator>/usr/bin/qemu-system-x86_64</emulator>
            <disk type='network' device='disk'>
                <driver name='qemu' type='raw'/>
                <source protocol='openvstorage' name='vol1' snapshot-timeout='120'>
                    <host name='ovs.include.gr' port='12329'/>
                </source>
                <target dev='vda' bus='virtio'/>
            </disk>
            <controller type='usb' index='0'/>
            <controller type='ide' index='0'/>
            <controller type='pci' index='0' model='pci-root'/>
            <memballoon model='virtio'/>
        </devices>
    </domain>


This will result in these QEMU args:


    LC_ALL=C PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games HOME=/home/cnanakos USER=cnanakos LOGNAME=cnanakos QEMU_AUDIO_DRV=none 
    /usr/bin/qemu-system-x86_64 -name QEMUGuest1 -S -machine pc,accel=tcg,usb=off -cpu qemu32 -m 214 -realtime mlock=off -smp 1,sockets=1,cores=1,threads=1 
    -uuid c7a5fdbd-edaf-9455-926a-d65c16db1809 -nographic -no-user-config -nodefaults -chardev socket,id=charmonitor,path=/home/cnanakos/.config/libvirt/qemu/lib/QEMUGuest1.monitor,server,nowait
    -mon chardev=charmonitor,id=monitor,mode=control -rtc base=utc -no-shutdown -no-acpi -boot strict=on -device piix3-usb-uhci,id=usb,bus=pci.0,addr=0x1.0x2 
    -drive file=openvstorage+tcp:ovs.include.gr:12329/vol1:snapshot-timeout=120,if=none,id=drive-virtio-disk0,format=raw -device virtio-blk-pci,scsi=off,bus=pci.0,addr=0x2,drive=drive-virtio-disk0,id=virtio-disk0,bootindex=1
    -device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x3 


The default transport type when a host is specified is 'tcp'. If someone needs to define the transport type the XML definition will look like this:

    <domain type='qemu'>
        <name>QEMUGuest1</name>
        <uuid>c7a5fdbd-edaf-9455-926a-d65c16db1809</uuid>
        <memory unit='KiB'>219136</memory>
        <currentMemory unit='KiB'>219136</currentMemory>
        <vcpu placement='static'>1</vcpu>
        <os> <type arch='i686' machine='pc'>hvm</type> <boot dev='hd'/> </os>
        <clock offset='utc'/>
        <on_poweroff>destroy</on_poweroff>
        <on_reboot>restart</on_reboot>
        <on_crash>destroy</on_crash>
        <devices>
            <emulator>/usr/bin/qemu-system-x86_64</emulator>
            <disk type='network' device='disk'>
                <driver name='qemu' type='raw'/>
                <source protocol='openvstorage' name='vol1' snapshot-timeout='120'>
                    <host name='ovs.include.gr' port='12329' transport='rdma'/>
                </source>
                <target dev='vda' bus='virtio'/>
            </disk>
            <controller type='usb' index='0'/>
            <controller type='ide' index='0'/>
            <controller type='pci' index='0' model='pci-root'/>
            <memballoon model='virtio'/>
        </devices>
    </domain>

that will give a result:

    LC_ALL=C PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games HOME=/home/cnanakos USER=cnanakos LOGNAME=cnanakos QEMU_AUDIO_DRV=none
    /usr/bin/qemu-system-x86_64 -name QEMUGuest1 -S -machine pc,accel=tcg,usb=off -cpu qemu32 -m 214 -realtime mlock=off -smp 1,sockets=1,cores=1,threads=1
    -uuid c7a5fdbd-edaf-9455-926a-d65c16db1809 -nographic -no-user-config -nodefaults -chardev socket,id=charmonitor,path=/home/cnanakos/.config/libvirt/qemu/lib/QEMUGuest1.monitor,server,nowait
    -mon chardev=charmonitor,id=monitor,mode=control -rtc base=utc -no-shutdown -no-acpi -boot strict=on -device piix3-usb-uhci,id=usb,bus=pci.0,addr=0x1.0x2
    -drive file=openvstorage+rdma:ovs.include.gr:12329/vol1:snapshot-timeout=120,if=none,id=drive-virtio-disk0,format=raw -device virtio-blk-pci,scsi=off,bus=pci.0,addr=0x2,drive=drive-virtio-disk0,id=virtio-disk0,bootindex=1
    -device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x3

For shared memory communication a host should not be specified at all, an example follows:

    <domain type='qemu'>
        <name>QEMUGuest1</name>
        <uuid>c7a5fdbd-edaf-9455-926a-d65c16db1809</uuid>
        <memory unit='KiB'>219136</memory>
        <currentMemory unit='KiB'>219136</currentMemory>
        <vcpu placement='static'>1</vcpu>
        <os> <type arch='i686' machine='pc'>hvm</type> <boot dev='hd'/> </os>
        <clock offset='utc'/>
        <on_poweroff>destroy</on_poweroff>
        <on_reboot>restart</on_reboot>
        <on_crash>destroy</on_crash>
        <devices>
            <emulator>/usr/bin/qemu-system-x86_64</emulator>
            <disk type='block' device='disk'>
                <driver name='qemu' type='raw'/> 
                <source dev='/dev/HostVG/QEMUGuest1'/>
                <target dev='hda' bus='ide'/>
                <address type='drive' controller='0' bus='0' target='0' unit='0'/>
            </disk>
            <disk type='network' device='disk'>
                <driver name='qemu' type='raw'/>
                <source protocol='openvstorage' name='image' snapshot-timeout='98765'/>
                <target dev='vda' bus='virtio'/>
            </disk>
            <controller type='usb' index='0'/>
            <controller type='ide' index='0'/>
            <controller type='pci' index='0' model='pci-root'/>
            <memballoon model='virtio'/>
        </devices>
    </domain>

and the args result will be:

    LC_ALL=C PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games HOME=/home/cnanakos USER=cnanakos LOGNAME=cnanakos QEMU_AUDIO_DRV=none
    /usr/bin/qemu-system-x86_64 -name QEMUGuest1 -S -machine pc,accel=tcg,usb=off -cpu qemu32 -m 214 -realtime mlock=off -smp 1,sockets=1,cores=1,threads=1
    -uuid c7a5fdbd-edaf-9455-926a-d65c16db1809 -nographic -no-user-config -nodefaults -chardev socket,id=charmonitor,path=/home/cnanakos/.config/libvirt/qemu/lib/QEMUGuest1.monitor,server,nowait
    -mon chardev=charmonitor,id=monitor,mode=control -rtc base=utc -no-shutdown -no-acpi -boot strict=on -device piix3-usb-uhci,id=usb,bus=pci.0,addr=0x1.0x2
    -drive file=/dev/HostVG/QEMUGuest1,if=none,id=drive-ide0-0-0,format=raw -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1
    -drive file=openvstorage:image:snapshot-timeout=98765,if=none,id=drive-virtio-disk0,format=raw -device virtio-blk-pci,scsi=off,bus=pci.0,addr=0x2,drive=drive-virtio-disk0,id=virtio-disk0
    -device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x3 


### blktap
Blktap block device on OpenvStorage volume can be attached like this:

    tap-ctl create -a openvstorage[+transport]:[hostname[:port]/]volume_name

Examples:

    tap-ctl create -a openvstorage+tcp:localhost:12345/volume

    tap-ctl create -a openvstorage+tcp:localhost/volume

    tap-ctl create -a openvstorage+rdma:localhost:12345/volume

    tap-ctl create -a openvstorage+rdma:localhost/volume

    tap-ctl create -a openvstorage:volume


### TGT
TGT volume on OpenvStorage can be specified like this:

    tgtadm --lld iscsi --op new --bstype openvstorage[+transport] --mode logicalunit --tid 1 --lun 1 -b [host:[port]/]volume_name

Examples:

    tgtadm --lld iscsi --op new --bstype openvstorage+tcp --mode logicalunit --tid 1 --lun 1 -b localhost:12345/volume

    tgtadm --lld iscsi --op new --bstype openvstorage+tcp --mode logicalunit --tid 1 --lun 1 -b localhost/volume

    tgtadm --lld iscsi --op new --bstype openvstorage+rdma --mode logicalunit --tid 1 --lun 1 -b localhost:12345/volume

    tgtadm --lld iscsi --op new --bstype openvstorage+rdma --mode logicalunit --tid 1 --lun 1 -b localhost/volume

    tgtadm --lld iscsi --op new --bstype openvstorage --mode logicalunit --tid 1 --lun 1 -b volume

