## FileDriverDesign
### API
* create(ContainerId)
* delete(ContainerId)
* stop(ContainerId)
* restart(ContainerId)
* setPolicy(ContainerId, policy)
* cleanup()
* read(ContainerId, buf, offset)
* write(ContainerId, buf, offset)
* truncate(ContainerId, size)
* sync(ContainerId)
* get_size(ContainerId)

### Design choices

File data is split up in extents of say, 4MiB, which map to individual object on the backend. The position of the extent within the file is implicitly defined by its object name in the backend, e.g. with 4-byte hex names (might be prefixed with containerid, see further).
Extents can be smaller than 4MiB to not waste resources on small files. Not only the last extent can be smaller than 4MiB to support files with holes, cheap truncation (increase size without having to create intermediate extents or inflate them). Extents cannot be empty (see further). Coordination between nodes needs to be build on top, filedriver can assume it has exclusive access to a file: no split brain protection.

#### Caching, backend consistency

Changes to extents are done on a cached version and applied to the backend asynchronously, with a configurable maximum delay (say 10 s default).

#### Namespace per file or global

Namespace per file:
* matches volumedriver volume per namespace (+)
* creation and deletion of file have explicit counterparts on backend => exists call can be easily implemented (+)
* depending on backend, namespaces might not come that cheap (e.g. 100 bucket limitation on S3) (-)

One global namespace and use prefixes to separate objects belonging to different files:
* in case no special metadata-object per file is used, an empty file has no object at all on backend: no difference between empty and non-existing file on backend (+) (-)
* in case special metadatafile is used, more room for backend inconsistencies: orphaned objects belonging to a file without a metadata-object (-)

#### Explicit or implicit size on backend

##### Explicit size

Size is stored separately on backend (in some meta file).
* atomic truncate for both shrinking and increasing  (given GC) (+)
* need for GC (-)
* ambiguous backend state (extents beyond size should be ignored and GC'd) (-)
* append write requires two updates to the backend: data and metadata (-)

##### Implicit size

Size is defined as largest addressable cluster in any extent. 0 in case no extents exists. Definition works best if empty extents are not allowed.

More precisely. Let each extent store at max K clusters. Let extents be numbered 0, 1, ... resp. representing data \[0,K\[ \[K, 2K\[ etc. For an existing extent i, let Size( i) be the number of clusters in that extent. We assume Size( i) > 0. Let E be the set of existing extent numbers of a file. Then the size of the file is defined as:
```
max_\{i in E \} ( K * i + Size( i) )
```
With this definition the size of an empty file is not a special case as 0 is the neutral element of the max operator on nonnegative numbers.

If we allow empty containers the definition is still unambiguous, but any file with size multiple of K will have two valid representations which smells a bit:
* E = \{a, ..., b} with Size(b) == K
* E = \{a, ..., b+1} with Size(b+1) ==0

Properties
* simple (+)
* data on backend completely and uniquely defines container, no storage leak, GC not necessary (+)
* no atomic truncate (on backend) (-)

##### Conclusion

* use implicitly defined size: on (re)start of a file, size is determined as described above (and further cached of course)
* don't allow empty extents=> when an extent would become empty it's deleted instead
* global namespace with prefixes
* without special metadata-object this implies
** restart of a container cannot fail
** no exists call possible (is defined by metadata)
** no difference between create and restart of an empty file

#### Restart

The filedriver has a concept of "running" containers, after startup, restart brings live a container, can reuse local extents from the cache
(unified restart-from-backend / local restart).

So typical startup sequence on node X
* for each container C registered on node X:
** X.restart(C)
* X.cleanup()
=> unify with current filesystem/vrouter restart

### Thread Model

#### Exploration of an Asynchronous, Lockless Design

An asynchronous/reactive paradigm may have its advantages conceptually and performance-wise over a multithreaded, shared-state and lock based implementation for the task at hand.
In the first place it might interface directly with the message-based internode volumerouter communication and allow more parallelism there without having to resort to a threadpool.
The following is a sketch of what a reactive filedriver might look like. Under tight time constraints, however, it might be more wise to go for a more traditional, lock based, implementation.

Purpose
* no shared state between threads => no locks necessary
* message based communication between threads

Actors / threads
* front-handler: handles incoming IO and mgmt requests and translates them into extent-tasks
* backend-workers: does puts gets and deletes on backend
* local-workers: performs IO on local cache: read, create, write, truncate

![](/Images/FileDriver-async-1.png)

```
There is no need to have distinct backend and local workers. The original reason for this distinction was that backend workers will typically be slower than local workers and that the work is dispatched round-robin from a DEALER socket to the workers. We can however build a simple load balancing mechanism based on a ROUTER that dispatches to a set of known workers. !FileDriver-async-2.png|border=1!
```

We have the following backend and local tasks:
```
BACK_PUT
BACK_GET
BACK_DELETE
LOCAL_WRITE (handles creation if not cached)
LOCAL_READ
LOCAL_DELETE
LOCAL_TRUNCATE  (handles creation if not cached)
```

Per extent, only 1 BACK_\* task can be in flight. LOCAL_READ, LOCAL_WRITE can be done in parallel, but LOCAL_TRUNCATE and LOCAL_DELETE need to run exclusively.

**Front handler**

For each container, the FileDriver keeps the following cached in memory:
* its size
* a vector of extent size and status: tentative list
** ABSENT: extent does not exist
** BACKEND: extent exists but is not cached
** FETCH: extent exists but is not cached, a BACK_GET request is in flight
** CACHED: cached and clean
** DIRTY local_version timestamp: extent exists, is loaded in cache and either does not exist on backend yet or differs from backend, timestamp is oldest update on extent which made it dirty
** DIRTY_WB version_local version_in_flight timestamp: idem, and BACK_PUT is in flight
* for each extent a list of subrequestID's that depend on that extent

Implementation-wise it might turn out easier to just keep track of any in-flight task related to an extent instead of encoding that explicitly in the state. E.g. FETCH could just be <BACKEND, \[BACK_GET\]\> where the in-flight BACK-GET task indicates the fetch.

Request are split-up into sub-requests (one request per extent; with request size < extent size (does this hold?) this translates to at most 2 subrequests (enforce this?)), subrequests have status
| Ready | read or write has finished |
| Pending | either still waiting for an extent to arrive from the backend or waiting for a local IO to finish |

We have a memory structure to bookkeep in-flight (sub)requests (with a free-list). If all spaces are occupied we do not process further incoming requests.

When all subrequest are ready, the request can be responded to.

The front-handler listens to a {{front-io}} socket receiving requests:
* read(ContainerId, offset, size)
* write(ContainerId, offset, buf, size)
* sync(ContainerId)
* truncate(ContainerId, size)
* get_size(ContainerId)

and a {{front-mgmt}} socket receiving requests:
* create(ContainerId)
* delete(ContainerId)
* stop(ContainerId)

```
The split into a mgmt and an io socket is not strictly necessary, a single socket is sufficient.
```

Further we have a {{backend-back}} socket to which it sends actions to be done on the backend and a {{local-sock}} socket to which it sends actions to be done on cached extents.

There can be further fan-out to multiple workers, local and backend, so the main cannot assume a simple req-rep response pair.

The main thread polls its 4 sockets and first handles worker events, then frontend events.

##### Event handling examples

Front IO requests are split up in requests per extent. If requestsize < extentsize an IO action will involve at most two extents (not for truncate).

###### {{front-io}} READ(ContainerId, offset, size)

* the next free inflight request ID is gotten
* is split up in requests aligned per extent
* is registered in the in-flight requests
* for each subrequest-extent involved we check its status:
** ABSENT. this subrequest is marked Ready, the result is filled in (0's)
** BACKEND => FETCH. request is marked as pending, BACK_GET is sent, subrequestID is added to subscribers for that extent
** FETCH. marked as pending, this subrequestID is added to subscribers for extent being fetched
** CACHED \| DIRTY _ _ \| DIRTY_WB _ _ \_. marked as pending, LOCAL_READ read request is sent to the {{local-sock}}
* if all subrequests are handled, a read response is sent to {{front-io}}

###### {{front-io}} TRUNCATE(ContainerId, offset, size)

Is translated into subrequests per extent. In case of shrinking these are deletes and potentially 1 truncate.
In case of growing translates into a truncate on an extent.
For delete actions on an extent:
* ABSENT. impossible (should not have triggered delete in the first place)
* BACKEND. marked as pending, schedule BACK_DELETE. subrequestID is added to subscribers for that extent
* FETCH. marked as pending, subrequestID is added to subscribers for extent being fetched
* CACHED \| DIRTY _ _ \| DIRTY_WB _ _ \_. marked as pending.  LOCAL_DELETE is scheduled. subrequestID is added to subscribers for extent

###### {{front-io}} WRITE(ContainerId, offset, size)

* the next free inflight request ID is gotten
* is split up in requests aligned per extent (will typically only be 1 extent and at most 2 (given requestsize < extentsize ))
* is registered in the in-flight requests
* for each subrequest-extent involved we check its status:
** ABSENT. LOCAL_WRITE is sent to {{local-sock}}
** BACKEND => FETCH. request is marked as pending, BACK_GET is sent, subrequestID is added to subscribers for that extent
** FETCH. marked as pending, this subrequestID is added to subscribers for extent being fetched
** CACHED \| DIRTY _ _ \| DIRTY_WB _ _ \_. LOCAL_WRITE is sent to {{local-sock}}

###### finished LOCAL_READ

* subrequest is marked as ready
* subrequestID is removed from subscribers of extent
* if all subrequests of the request are handled, request is finished (a read response is sent to {{front-io}}

###### finished LOCAL_WRITE

If extent status is:
* ABSENT => DIRTY now()
* BACKEND (should not happen as local deletes cannot run in parallel with local writes)
* CACHED => DIRTY now()
* DIRTY v t => DIRTY v+1 t1
* DIRTY_WB local_version version_in_flight t => DIRTY_WB local_version+1 version_in_flight (local_version == version_in_flight ? now() : t1)

Furthermore:
* subrequest is marked as ready
* subrequestID is removed from subscribers of extent
* if all subrequests of the request are handled, request is finished (a read response is sent to {{front-io}}

###### finished BACK_GET

if extent status is:
* ABSENT (cannot happen)
* BACKEND => CACHED
* CACHED \| DIRTY \| DIRTY_WB (cannot happen)
Furthermore the list of subscribers for that extent is scanned and further actions taken, e.g. reads writes can be scheduled in parallel, a lingering delete originating from a truncate need to wait.

###### finished BACK_PUT

if extent status is:
* ABSENT \| FETCH \| CACHED \| DIRTY: (cannot happen)
* DIRTY_WB local_version version_inflight t: (local_version == version_inflight ? CACHED : (DIRTY local_version t))

##### Backend scheduler

For extents in status DIRTY v t, beyond their expiry time, are scheduled to the backend with a BACK_PUT request:
* DIRTY v t => DIRTY_WB v v t

Their status is set to DIRTY_WB immediately, such that:
* the next incoming write can update the timestamp
* no further writes will update the timestamp

##### Cache cleaner

Only CACHED extents without subscribers are eligible for eviction from the cache by issuing a LOCAL_DELETE action.

#### Conservative Design

Client threads (either from FUSE/Ganesha or the ObjectRouter) call directly into the FileDriver; access is serialized based on locks instead of sending messages to a single frontend thread dispatching between a ROUTER socket and worker threads.
* *Initially* the worker threads are only used for backend access (the implementation could use the existing threadpool or be based on ØMQ); if it turns out that requests spanning multiple extents are bottlenecked by this, cache operations could also be offloaded to worker threads so they can be processed concurrently.
* The extent states and transitions between these outlined in the async design above need to be implemented here as well.
* The subscriber list of an extent contains condition variables. Once a backend task (PUT, GET, DELETE) finishes, the condvars are signalled and removed from the list. {note}TBD: use promises / futures instead, as these avoid certain problems / allow error propagation{note}

##### Examples

{note}We use the term "subrequests" here as well to show similarities with the async approach while at the same time hopefully avoiding confusion.
{note}

###### READ(ContainerId, offset, buf)

* create a condition variable CV
* create a number of subrequests on extent boundaries and put them on a list
* while list is not empty:
** get list head, check extent state:
*** ABSENT: associated buffer part is zeroed, subrequest is removed from list
*** BACKEND: set state to FETCH, CV is added to subscriber list of extent, schedule backend fetch (which transitions the extent to CACHED)
*** FETCH: add CV to subscriber list of extent if not present already
** CACHED \| DIRTY \| DIRTY_WB: read from cache to buffer, drop subrequst from list
** sleep on CV

###### WRITE(ContainerId, offset, buf)

* create a condition variable CV
* create a number of subrequests on extent boundaries and put them on a list
* while list is not empty:
** get list head, check extent state:
*** ABSENT: write to cache, mark as DIRTY subrequest is removed from list
*** BACKEND: set state to FETCH, CV is added to subscriber list of extent, schedule backend fetch (which transitions the extent to CACHED)
*** FETCH: add CV to subscriber list of extent if not present already
** CACHED \| DIRTY \| DIRTY_WB: write from buffer to cache, set to DIRTY if it was CACHED, drop subrequest from list
** sleep on CV

###### TRUNCATE(ContainerId, size)

* create a condition variable CV
* create subrequests, in case of shrinking this translates to DELETEs and TRUNCATE, in case of growing this translates to a TRUNCATE. Put DELETEs on a list
* while list is not empty:
** get list head, check extent state:
*** ABSENT: drop subrequest from list
*** BACKEND: set state to FETCH {warning}introduce DELETE state?{warning}, add CV to subscriber list of extent, schedule backend delete (which transitions the extent to ABSENT)
*** FETCH: add CV to subscriber list of extent if not present already
*** CACHED, DIRTY: remove from local cache, add CV to subscriber list of extent, schedule backend delete (which transitions the extent to ABSENT)
*** DIRTY_WB: add CV to subscriber list of extent if not present already
** sleep on CV
* carry out the final TRUNCATE:
** check extent state:
*** ABSENT: create it, mark as DIRTY
*** BACKEND: set state to FETCH, add CV to subscriber list of extent, schedule backend delete (which transitions the extent to ABSENT)
*** FETCH: add CV to subscriber list of extent if not present already
*** CACHED, DIRTY, DIRTY_WB: truncate extent and set to DIRTY

{warning}
TODO: Rule out modifications while DIRTY_WB\!
{warning}

##### Backend scheduler

Thread that walks over all extents and schedules backend put tasks for all DIRTY extents whose deadline has expired.

##### Cache Replacement Strategy

If the cache is full and a new extent is to be created / read from the backend, an old one needs to be evicted. This will be done synchronously by the thread requiring the storage space.
The replacement strategy to use is LRU, only CACHED extents participate. If no CACHED extents can be found a similar mechanism as above is used, i.e. a condition variable CV is put to the subscriber list of an extent that is being written out and the caller waits on the condition variable until it's signalled.

##### Locking

##### Error Handling

{warning}
* Don't wait indefinitely on condition variables, but signal timeouts\!?
{warning}

#### Conclusion

The simplified async approach and the conservative design expose on first sight a roughly equivalent complexity / implementation effort. The perceived advantage of the async approach of being able to hook into the internode protocol is currently not viable as the ObjectRouter is not fully based on ØMQ (local I/O uses function calls for historical and performance reasons), it does however avoid a complex locking scheme required by the conservative design.