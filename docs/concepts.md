# VolumeDriver concepts
The Volume Driver is the core of the Open vStorage solution. It provides the [Read Cache](#readcache) functionality, the [Write Buffers](#writebuffer), the [Distributed Transaction Log](#DTL) and the [metadata](#metadata).

{% include "writebuffer.md" %}

{% include "DTL.md" %}

{% include "readcache.md" %}

{% include "albaproxy.md" %}

{% include "shm.md" %}

{% include "fencing.md" %}

{% include "scrubber.md" %}
