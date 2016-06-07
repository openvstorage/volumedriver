# Open vStorage VolumeDriver Introduction
The Open vStorage VolumeDriver is the core of the Open vStorage solution: a high performance distributed block layer. It is the technology that converts block storage into object (Storage Container Objects) which can be stored on the different supported backends. To minimize the latency and boost the performance, it offers read and write acceleration on SSDs or PCI-e flash cards.

The VolumeDriver implements functionality such as zero-copy snapshots, thin-cloning, scrubbing of data out of the retention, thin provisioning and a distributed transaction log.



The VolumeDriver Documentation is divided into following sections:
* [Volume Driver Architecture](docs/architecture.md)
* [Volume Driver Concepts](docs/concepts.md)
* [Volume Driver Components](docs/components.md)
* [Volume Driver Python API](docs/pythonapi.md)
* [Volume Driver Configuration](docs/config.md)
* [VolumeDriver Events](docs/events.md)
* [VolumeDriver CLI](docs/volumedriver_fs_cli.md)

The Edge Documentation:
* [Edge Architecture](docs/edge.md)
* [Edge Interfaces](docs/edge_interfaces.md)