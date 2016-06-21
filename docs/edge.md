<a name="edge"></a>
## Edge
The Open vStorage Edge client library offers a block API at the front and in the back connects into the Open vStorage VolumeDriver.

![Edge diagram](https://drive.google.com/open?id=1PfCnvCGPlsdqNbIW6BWk8NxnFq21Q_qZ1ARjQB2oRD8)

The connection to the VolumeDriver can be done either using a local shared memory interface or a networked based tcp/ip or rdma connection.

{% include "edge_interfaces.md" %}