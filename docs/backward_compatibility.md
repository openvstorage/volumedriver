# VolumeDriver Backward Compatibility Notes

VolumeDriver tries to maintain backward compatibility by default. In general
it's desirable that new features can be introduced seamlessly to a cluster, i.e.
instances of old versions will still work side by side with instances of new
versions, with the latter instances being able to make use of the new feature.
It might however not always be possible to achieve this, in which case the
strategy is to make the use of the new feature configurable.

Such changes are listed here.

## 6.3.0

This version introduces HA support which necessitated a non-backward compatible
change in the inter-node communication. Support for this is *enabled by default*
and can be disabled with the `volume_router/vrouter_send_sync_response` key in
the [VolumeDriver configuration](config.md) if a cluster with older versions is
to be upgraded.
