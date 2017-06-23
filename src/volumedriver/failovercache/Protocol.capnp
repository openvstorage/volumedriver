# We're not using Capn'Proto's RPC but only the serialization, so we're also
# not using methods / not returning capabilities / don't map state to the latter.
# IOW the state machine ('open' before any other calls, no calls after
# 'close') needs to be implemented out of band.
# Bulk data (IOW actual ClusterEntry data) is not serialized
# - to avoid copying (which could be circumvented with a
#   SegmentedArrayMessageReader, but then segment info needs to be transferred)
# - to allow implementations on top of RDMA read/write for bulk data and RDMA
#   send/recv for the messages.

@0x82f2b337991d7d58;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("volumedriver::failovercache::protocol");

struct Request
{
    requestData @0 : RequestData;
}

struct RequestData
{
    union
    {
        openRequest @0 : OpenRequest;
        closeRequest @1 : CloseRequest;
        getRangeRequest @2 : GetRangeRequest;
        addEntriesRequest @3 : AddEntriesRequest;
        getEntriesRequest @4 : GetEntriesRequest;
        flushRequest @5 : FlushRequest;
        clearRequest @6 : ClearRequest;
        removeUpToRequest @7 : RemoveUpToRequest;
    }
}

struct OpenRequest
{
    nspace @0 : Text;
    clusterSize @1 : UInt32;
    ownerTag @2 : UInt64;
}

struct CloseRequest
{}

struct GetRangeRequest
{}

struct AddEntriesRequest
{
    entries @0 : List(ClusterEntry);
}

# startClusterLocation = ClusterLocation(0) -> start with first entry present
# numEntries: max number of entries to send, underflow indicates EOF
struct GetEntriesRequest
{
    startClusterLocation @0 : UInt64;
    numEntries @1 : UInt32;
}

struct FlushRequest
{}

struct ClearRequest
{}

struct RemoveUpToRequest
{
     clusterLocation @0 : UInt64;
}

struct Response
{
     rsp : union
     {
         ok @0 : ResponseData;
         error @1 : Error;
     }
}

struct ResponseData
{
     union
     {
         openResponse @0: OpenResponse;
         closeResponse @1 : CloseResponse;
         getRangeResponse @2 : GetRangeResponse;
         addEntriesResponse @3 : AddEntriesResponse;
         getEntriesResponse @4 : GetEntriesResponse;
         flushResponse @5 : FlushResponse;
         clearResponse @6 : ClearResponse;
         removeUpToResponse @7 : RemoveUpToResponse;
     }
}

struct OpenResponse
{}

struct CloseResponse
{}

# [ begin, end ]
# [ ClusterLocation(0), ClusterLocation(0) ] -> no entries present
struct GetRangeResponse
{
    begin @0 : UInt64 = 0;
    end @1 : UInt64 = 0;
}

struct AddEntriesResponse
{}

struct GetEntriesResponse
{
    entries @0 : List(ClusterEntry);
}

struct FlushResponse
{}

struct ClearResponse
{}

struct RemoveUpToResponse
{}

struct Error
{
    code @0 : UInt32 = 0;
    message @1 : Text;
}

struct ClusterEntry
{
    address @0 : UInt64;
    clusterLocation @1 : UInt64;
}
