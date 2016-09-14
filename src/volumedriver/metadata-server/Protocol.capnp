@0x9c0aa9911ddb5a13;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("metadata_server_protocol");

enum Role
{
    master @0;
    slave @1;
}

struct Record
{
    key @0 : Data;
    val @1 : Data;
}

struct Error
{
    message @0 : Text;
}

# Arrr matey, ye olde scumbag Cap'n P. insists on camelCase.

struct TableCounters
{
    totalTLogsRead @0 : UInt64;
    incrementalUpdates @1 : UInt64;
    fullRebuilds @2 : UInt64;
}

# We might want to avoid sending the `nspace' string but introduce another layer of
# indirection to avoid string comparisons and lookups - profile!

interface Methods
{
    drop @0 (nspace : Text) -> ();

    clear @1 (nspace : Text,
    	      ownerTag : UInt64 = 0) -> ();

    list @2 () -> (nspaces : List(Text));

    multiGet @3 (nspace : Text, keys : List(Data)) -> (values : List(Data));

    multiSet @4 (nspace : Text,
    	     	 records : List(Record),
		 barrier : Bool = false,
		 ownerTag : UInt64 = 0) -> ();

    setRole @5 (nspace : Text, role : Role,
    	        ownerTag : UInt64 = 0) -> ();

    getRole @6 (nspace : Text) -> (role : Role);

    open @ 7 (nspace : Text) -> ();

    ping @ 8 (data : Data) -> (data : Data);

    applyRelocationLogs @ 9 (nspace : Text,
    			     cloneId : UInt8,
    			     scrubId : Text,
			     logs : List(Text)) -> ();

    catchUp @ 10 (nspace : Text, dryRun : Bool) -> (numTLogs : UInt32);

    getTableCounters @ 11 (nspace : Text, reset : Bool) -> (counters : TableCounters);

    getOwnerTag @ 12 (nspace : Text) -> (ownerTag : UInt64);
}
