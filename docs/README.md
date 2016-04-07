# Open vStorage VolumeDriver
The Open vStorage VolumeDriver is the core of the Open vStorage solution: a high performance distributed block layer. It is the technology that converts block storage into object (Storage Container Objects) which can be stored on the different supported backends. To minimize the latency and boost the performance, it offers read and write acceleration on SSDs or PCI-e flash cards.

The VolumeDriver implements functionality such as zero-copy snapshots, thin-cloning, scrubbing of data out of the retention, thin provisioning and a distributed transaction log.

The VolumeDriver currently supports KVM (.raw) and VMware (.vmdk) as hypervisor.

The VolumeDriver consists out of 3 modules:
* The Virtual File System Router: a layer which routes IO to the correct Volume Router.
* The Volume Router: conversion from block into objects (Storage Container Objects), caching and scrubber functionality
* The Storage Router: spreading the SCOs across the storage backend.

The Open vStorage VolumeDriver is written in C++.