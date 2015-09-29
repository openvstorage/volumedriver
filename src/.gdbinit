 handle SIGUSR1 noprint nostop pass
 handle SIGPIPE noprint nostop pass
set environment PATH /home/immanuel/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/home/immanuel/workspace/code/3.2/VolumeDriver/target/bin

#set follow-fork-mode child
#set follow-exec-mode new
define only_test
set args --gtest_filter=$arg0
end

define bv_volumeconfig
call debug::DPVolumeConfig($arg0)
end

document bv_volumeconfig
"Prints a VolumeConfig* in human readable representation"
end

define bv_nsidmap
call debug::DPNSIDMap($arg0)
end

document bv_nsidmap
"Prints a NSIDMap* in a human readable representation"
end

define bv_clusterlocation
call debug::DPClusterLocation($arg0)
end

document bv_clusterlocation
"Prints a ClusterLocation* in a human readable representation"
end

define bv_cacheentry
call debug::DPCacheEntry($arg0)
end

document bv_cacheentry
"Prints a CacheEntry in a human readable representation"
end

define bv_failovercache
call debug::DPFailOverCache($arg0)
end

document bv_failovercache
"Prints a FailOverCache* in human readable representation"
end

define bvirtual
end

document bvirtual
"bv_volumeconfig"
"bv_nsidmap"
"bv_clusterlocation"
"bv_cacheentry"
"bv_failovercache"
end