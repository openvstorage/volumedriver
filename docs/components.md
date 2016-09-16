## VolumeDriver Components
The VolumeDriver consists out of 3 modules:
* The Virtual File System Router: a layer which routes IO to the correct Volume Router.
* The Volume Router: conversion from block into objects (Storage Container Objects), caching and scrubber functionality
* The Storage Router: spreading the SCOs across the storage backend.
