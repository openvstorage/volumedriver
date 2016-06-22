# Open vStorage VolumeDriver Introduction
The Open vStorage VolumeDriver is the core of the Open vStorage solution: a high performance distributed block layer. It is the technology that converts block storage into object (Storage Container Objects) which can be stored on the different supported backends. To minimize the latency and boost the performance, it offers read and write acceleration on SSDs or PCI-e flash cards.

The VolumeDriver implements functionality such as zero-copy snapshots, thin-cloning, scrubbing of data out of the retention, thin provisioning and a distributed transaction log.



The VolumeDriver Documentation is divided into following sections:
* [Architecture](docs/architecture.md)
* [Concepts](docs/concepts.md)
* [Components](docs/components.md)
* [Building the Volume Driver](docs/building.md)
* [Using the Volume Driver](docs/using.md)
* [Events](docs/events.md)
* [Troubleshooting](docs/troubleshooting.md)
    