Open vStorage VolumeDriver
==========================
The Open vStorage Volumedriver is the core of the Open vStorage solution: a high performance distributed block layer (optimised for virtual disks as used by KVM or VMware). It is the technology that converts block storage into object (Storage Container Objects) which can be stored on the different supported backends. To minimize the latency and boost the performance, it offers read and write acceleration on SSDs or PCI-e flash cards.

The Volumedriver implements functionality such as zero-copy snapshots, thin-cloning, scrubbing of data out of the retention, thin provisioning and a distributed transaction log.

The Volumedriver currently supports KVM (.raw) and VMware (.vmdk) as hypervisor; depending on its configuration. It can also store other small files, but it's not optimised for this (read: do not try to directly store a multigigabyte iso image on it).

The Volumedriver consists out of 3 modules:
* The Virtual File System Router: a layer which routes IO to the correct Volume Router.
* The Volume Router: conversion from block into objects (Storage Container Objects), caching and scrubber functionality
* The Storage Router: spreading the SCOs across the storage backend.

The Open vStorage Volumedriver is written in C++.

License
-------
GNU AGPLv3 (see LICENSE.txt) unless noted otherwise in the upstream components that
are part of this source tree (cppzmq, etcdcpp, msgpack-c, procon, pstreams, xmlrpc++).

Building
--------

The simplest and prefered way to build the volumedriver is using our [dockerised build process](doc/build_with_docker.md) that uses a clean container to hold build prerequisites, preventing conflicts with already installed software on your development machine. We advise you to start with that procedure. Afterwards, if you should want to switch to a bare metal build, details are in the [manual building](doc/manual_building.md) document.
