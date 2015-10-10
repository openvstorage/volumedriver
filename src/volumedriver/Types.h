// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TYPES_H_
#define TYPES_H_

#include "SnapshotName.h"

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

BOOLEAN_ENUM(DeleteLocalData);
BOOLEAN_ENUM(RemoveVolumeCompletely);
BOOLEAN_ENUM(ForceVolumeDeletion);
BOOLEAN_ENUM(DeleteVolumeNamespace);
BOOLEAN_ENUM(PrefetchVolumeData);
BOOLEAN_ENUM(IgnoreFOCIfUnreachable);
BOOLEAN_ENUM(FallBackToBackendRestart);
BOOLEAN_ENUM(CreateNamespace);
BOOLEAN_ENUM(DryRun);

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
class OrderedTLogNames : public std::vector<TLogName> {};
template<class Archive>
void serialize(Archive& ar,
               OrderedTLogNames& res,
               const unsigned)
{
    ar & static_cast<std::vector<TLogName>& >(res);
}

typedef SnapshotName SnapshotWorkUnit;

typedef std::vector<std::pair<SCOCloneID, OrderedTLogNames> > CloneTLogs;
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
                                 const OrderedTLogNames &n)
{
    OrderedTLogNames::const_iterator i = n.begin();
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
