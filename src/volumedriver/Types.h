// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#ifndef TYPES_H_
#define TYPES_H_

#include "SnapshotName.h"
#include "TLogId.h"

#include <stdint.h>

#include <list>
#include <iosfwd>
#include <string>
#include <vector>

#include <boost/intrusive_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/CheckSum.h>
#include <youtils/ConfigurationReport.h>
#include <youtils/IncrementalChecksum.h>
#include <youtils/OurStrongTypedef.h>
#include <youtils/RWLock.h>
#include <youtils/StrongTypedString.h>
#include <youtils/UUID.h>

#include <backend/BackendInterface.h>

STRONG_TYPED_STRING(volumedriver, VolumeId);

namespace youtils
{
class UUID;
class Guid;
class CheckSum;
class CheckSumException;
class ConfigurationProblem;
class UpdateReport;
class VolumeDriverComponent;
class FileUtils;
}

namespace backend
{
class BackendConfig;
typedef std::unique_ptr<BackendInterface> BackendInterfacePtr;
class BackendConnectionManager;
}

OUR_STRONG_ARITHMETIC_TYPEDEF(uint32_t, ClusterMultiplier, volumedriver)
OUR_STRONG_ARITHMETIC_TYPEDEF(uint32_t, ClusterSize, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, VolumeSize, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint32_t, SCOMultiplier, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint32_t, LBASize, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint8_t, SCOCloneID, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint8_t, SCOVersion, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint32_t, TLogMultiplier, volumedriver);
OUR_STRONG_ARITHMETIC_TYPEDEF(float, SCOCacheNonDisposableFactor, volumedriver);

inline volumedriver::SCOCloneID operator"" _scocloneid(unsigned long long n)
{
    return volumedriver::SCOCloneID(n);
}

inline volumedriver::SCOVersion operator"" _scoversion(unsigned long long n)
{
    return volumedriver::SCOVersion(n);
}

namespace volumedriver
{

typedef uint64_t PageAddress;

VD_BOOLEAN_ENUM(DeleteLocalData);
VD_BOOLEAN_ENUM(RemoveVolumeCompletely);
VD_BOOLEAN_ENUM(ForceVolumeDeletion);
VD_BOOLEAN_ENUM(DeleteVolumeNamespace);
VD_BOOLEAN_ENUM(PrefetchVolumeData);
VD_BOOLEAN_ENUM(IgnoreFOCIfUnreachable);
VD_BOOLEAN_ENUM(FallBackToBackendRestart);
VD_BOOLEAN_ENUM(CreateNamespace);
VD_BOOLEAN_ENUM(DryRun);
VD_BOOLEAN_ENUM(Reset);

// AR: these have to go
#if 1
namespace fs = boost::filesystem;
using youtils::UUID;
using youtils::CheckSum;
using youtils::CheckSumException;
using youtils::IncrementalChecksum;
using youtils::ConfigurationReport;
using youtils::ConfigurationProblem;
using youtils::UpdateReport;
using youtils::VolumeDriverComponent;
using youtils::FileUtils;

using backend::BackendConfig;
using backend::BackendInterface;
using backend::BackendInterfacePtr;
using backend::BackendConnectionManager;
using backend::Namespace;
#endif

typedef uint64_t ClusterAddress;
typedef uint32_t ClusterExponent;
typedef uint32_t SCONumber;
typedef uint16_t SCOOffset;
typedef uint64_t TlogCounter;

/**
 * snapshot number as used when a snapshot is created and as lives
 * in the TLog SNAPSHOT entry.
 */
typedef uint64_t SnapshotNum;
//typedef uint64_t ClusterLocationInt;
typedef boost::optional<CheckSum> MaybeCheckSum;

typedef uint64_t timestamp_t;

class TLogBackendReader;

typedef std::string TLogName;

typedef SnapshotName SnapshotWorkUnit;

typedef std::vector<std::pair<SCOCloneID, OrderedTLogIds> > CloneTLogs;

typedef std::vector<SnapshotWorkUnit> SnapshotWork;

class CachedSCO;
typedef boost::intrusive_ptr<CachedSCO> CachedSCOPtr;
typedef std::list<CachedSCOPtr> CachedSCOPtrList;

class OpenSCO;
typedef boost::intrusive_ptr<OpenSCO> OpenSCOPtr;

class SCOCacheMountPoint;
typedef boost::intrusive_ptr<SCOCacheMountPoint> SCOCacheMountPointPtr;

// next doesn't work for some reason in combination with logging :(
inline std::ostream & operator<<(std::ostream &o,
                                 const OrderedTLogIds &n)
{
    OrderedTLogIds::const_iterator i = n.begin();
    o << "[";
    while (i != n.end())
    {
        o << *i;
        o << ",";
        ++i;

    }
    o << "]\n";
    return o;
}

template<typename T>
class Accessor
{
public:
    Accessor() :
        lock_("Accessor"),
        x_()
    {}

    Accessor(const Accessor<T>& other) :
        lock_("Accessor"),
        x_(other.get())
    {}

    explicit Accessor(const T& xp) :
        lock_("Accessor"),
        x_(xp)
    {}

    T get() const {
        fungi::ScopedReadLock rl_(lock_);
        return x_;
    }

    T put(const T& xp) {
        fungi::ScopedWriteLock wl_(lock_);
        T tmp = x_;
        x_ = xp;
        return tmp;
    }

private:
    mutable fungi::RWLock lock_;
    T x_;
};

inline const char*
snapshotFilename()
{
    return "snapshots.xml";
}

}

#endif // TYPES_H_

// namespace volumedriver
// Local Variables: **
// mode: c++ **
// End: **
