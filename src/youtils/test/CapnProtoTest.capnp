@0xd2126981b66bb6d9;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("youtilstest::capnp_test");

struct Descriptor
{
    seqNum @0 : UInt64;
    data @1 : Data;
}

interface Methods
{
    nop @0 () -> ();

    write @1 (descs : List(Descriptor)) -> ();
}
