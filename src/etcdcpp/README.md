# etcdcpp

The following abstractions are available to access the etcd API:

a) etcd::Client implements the main client interface.

b) etcd::Watch implements a callback based watchdog to watch etcd key and directory changes.
 
Response from etcd is JSON. This implementation is agnostic to any specific json library. If you already have a json library in your project, just implement a wrapper simalar to one in "rapid_reply.hpp". If you would like to pick another JSON implementation, here: https://github.com/miloyip/nativejson-benchmark would be a good place to start.

## Key Space Operations

Create a client object
```cpp
etcd::Client<example::RapidReply> etcd_client("172.20.20.11", 2379);
```
### Setting the value of a key

```cpp
example::Reply reply = etcd_client.Set("/message", "Hello world");
```
### Get the value of a key

```cpp
example::RapidReply reply = etcd_client.Get("/message");
```

### Changing the value of a key

```cpp
example::RapidReply reply = etcd_client.Set("/message", "Hello etcd");
```
### Deleting a key

```cpp
example::RapidReply reply = etcd_client.Delete("/message");
```
### Using key TTL

```cpp
example::RapidReply reply = etcd_client.Set("/foo", "bar", 5);
```

If you try to get an expired key:

```cpp
example::RapidReply reply = etcd_client.Get("/foo");
```

The example rapid reply wrapper, we parse for an error and throw an exception of type etcd::ReplyException
```sh
terminate called after throwing an instance of 'etcd::ReplyException' 
  what():  Key not found[100]: /foo   
```

The TTL can be unset to avoid expiration through update operation:

```cpp
example::RapidReply reply = etcd_client.ClearTtl("/foo", "bar");
```
### Waiting for a change

```cpp
// Create a watch callback function
void WatchCallback(const example::RapidReply& reply) {
    reply.Print();
}

// Create a etcd::Watch object and call Run.
etcd::Watch<example::RapidReply> etcd_watchdog("172.20.20.11", 2379);
etcd_watchdog.Run("/foo", WatchCallback);

// If you want to watch a key or directory only once, use the RunOnce api call
etcd_watchdog.RunOnce("/foo", WatchCallback);
```

#### Specify a prevIndex for the watch

```cpp
etcd_watchdog.RunOnce("/foo", WatchCallback, 8);
```
#### Watch from cleared event index

etcd::Watch::Run will look for an index is outdated error and automatically reconfigure the watch

#### Connection being closed prematurely

etcd::Watch::Run will throw an etcd::ClientException when it looses connectivity.

### Atomically Creating In-Order Keys

```cpp
example::RapidReply reply = etcd_client.SetOrdered("/queue", "Job1");
```
#### To enumerate the in-order keys as a sorted list, use the "sorted" parameter.

```cpp
example::RapidReply reply  = etcd_client.GetOrdered("/queue");
```
### Using a directory TTL

```cpp
example::RapidReply reply = etcd_client.AddDirectory("/dir", 30);
```
#### Update a directory TTL
```cpp
example::RapidReply reply = etcd_client.UpdateDirectoryTtl("/dir", 30);
```
### Atomic Compare-and-Swap

#### Add only if a key value does not exist already.
```cpp
example::RapidReply reply = etcd_client.CompareAndSwapIf("/foo", "three", bool(false));
```
If it exists, the example::RapidReply wrapper also throws an exception

```sh
terminate called after throwing an instance of 'etcd::ReplyException'                                    │
  what():  Key already exists[105]: /foo
```

#### Add key only if previous value is equal to the specified value

```cpp
example::RapidReply reply = etcd_client.CompareAndSwapIf("/foo", "three", std::string("two"));
```
If the comparation fails, the example::RapidReply wrapper also throws an exception

```sh
terminate called after throwing an instance of 'etcd::ReplyException'                                    │
  what():  Compare failed[101]: [two != three] 
```
#### Add key only if previous index matches the specified value
```cpp
example::RapidReply reply = etcd_client.CompareAndSwapIf("/foo", "three", etcd::Index(7))
```

### Atomic Compare-and-Delete

#### Delete key only if value matches the specified value
```cpp
example::RapidReply reply = etcd_client.CompareAndDeleteIf("/foo", std::string("two"));
```

The example::RapidReply wrapper throws an exception if the value is not equal

```sh
terminate called after throwing an instance of 'etcd::ReplyException'                                    │
  what():  Compare failed[101]: [two != one]
```
As does a `CompareAndDelete` with a mismatched `prevIndex`:

#### Delete key only if the modifiedIndex is equal to the specified index
```sh
example::RapidReply reply = etcd_client.CompareAndDeleteIf("/foo", etcd::Index(7));
```
The example::RapidReply wrapper throws an exception if there is a mismatch in index

```sh
terminate called after throwing an instance of 'etcd::ReplyException'                                    │
  what():  Compare failed[101]: [1 != 8]  
```
### Creating Directories

```cpp
example::RapidReply reply = etcd_client.AddDirectory("/dir");
```
### Listing a directory
```cpp
example::RapidReply reply = etcd_client.Get("/")
```

#### Recursively get all the contents

```cpp
example::RapidReply reply = etcd_client.GetAll("/");
```
### Deleting a Directory

```cpp
example::RapidReply reply = etcd_client.DeleteDirectory("/foo_dir");
```
#### To delete a directory that holds keys, set recursive to true

```cpp
example::RapidReply reply = etcd_client.DeleteDirectory("/foo_dir", true);
```
### Creating a hidden node

```cpp
example::Reply reply = etcd_client.Set("/_message", "Hello hidden world");
```
### Setting a key from a file

Read the contents of the file into a string "value" and encode it

```cpp
example::Reply reply = etcd_client.Set(etcd_client.UrlEncode(value));
```
To unescape a string
```cpp
std::string decoded_value = etcd_client.UrlDecode(value))
```
