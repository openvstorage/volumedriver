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

#ifndef TCBTMETADATASTORESIM_H_
#define TCBTMETADATASTORESIM_H_

// Y42 make hell freeze over at the end of the week
#ifdef HELL_FREEZES_OVER
#include <ListContainingMap.h>
#include "GlobalMeasurements.h"
#include "MetaDataStoreInterface.h"

#include <youtils/IOException.h>
#include <youtils/Mutex.h>
#include <youtils/File.h>

#include <list>
#include <exception>
#include <string>
#include <tcutil.h>
#include <tcfdb.h>
#include <memory>
#include <exception>
#include <sstream>
#include <tcbdb.h>
#include "Types.h"


#define VOLUME_DRIVER_WRITE_SYNC_COUNT (10000)

namespace volumedriver {

class TCBTMetaDataStoreSimConfig: public MetaDataStoreInterfaceConfig {
public:
    std::string metaDataStorePath;
    uint32_t numberOfEntries;
    uint32_t numberOfMembersInLeafPage;
    uint32_t numberOfMembersInNonLeafPage;
    uint32_t numberOfElementsInBucketArray;
    uint32_t recordAlignmentByPowerOfTwo;
    int32_t maximumNumberInFreeBlockByPowerOfTwo;
    uint32_t maximumNumberOfLeafNodesCached;
    uint32_t maximumNumberOfNonLeafNodesCached;
    uint32_t internalCacheSize;

};

inline TCBTMetaDataStoreSimConfig makeDefaultTCBTMetaDataStoreSimConfig() {
    TCBTMetaDataStoreSimConfig theConfig;
    theConfig.numberOfMembersInLeafPage = 0;
    theConfig.numberOfMembersInNonLeafPage = 0;
    theConfig.numberOfElementsInBucketArray = 0;
    theConfig.recordAlignmentByPowerOfTwo = 3;
    theConfig.maximumNumberInFreeBlockByPowerOfTwo = -1;
    theConfig.maximumNumberOfLeafNodesCached = 0;
    theConfig.maximumNumberOfNonLeafNodesCached = 0;
    theConfig.internalCacheSize = 2;
    return theConfig;
}

class TCBTMetaDataStoreSimException: public MetaDataStoreException {
public:
    TCBTMetaDataStoreSimException(const std::string& i_msg)
        : MetaDataStoreException(i_msg)
    {}

    TCBTMetaDataStoreSimException(TCBDB* database, const char* function)
        : MetaDataStoreException("MetaDataStore TokyoCabinet Exception in: ")
    {
        msg_ += function;
        msg_ += " ";
        msg_ += tcbdberrmsg(tcbdbecode(database));
    }
};

template<int numBits, bool RangedPageAccess, bool CacheDebugging>
struct TCBTMetaDataPageTraits {
    static uint32_t getPageAddress(const ClusterAddress i_loc) {
        return (i_loc >> pageBits_);
    }

    static uint32_t getPageOffset(const ClusterAddress i_loc) {
        return (i_loc bitand (pageEntries_ - 1));
    }
    static const uint32_t pageBits_ = numBits;

    static const uint32_t pageEntries_ = 1 << pageBits_;

    static const uint32_t pageSize_ = pageEntries_ * sizeof(ClusterLocation);

    class Page
    {
    public:
        Page(void* p = 0) :
            dirty_(false),
            numTimesAccessed_(0), page_(0) {
            if (not p) {
                page_ = static_cast<ClusterLocation*> (calloc(TCBTMetaDataPageTraits::pageEntries_,
                                                              sizeof(ClusterLocation)));
                if (!page_)
                {
                    LOG_ERROR("Could not allocat new page");
                    throw TCBTMetaDataStoreSimException("Page::Page() could not allocate memory");
                }

                LOG_TRACE("Allocated " << page_);

            }
            else
            {
                page_ = static_cast<ClusterLocation*> (p);
                LOG_TRACE("Gotten pre-allocated " << page_);
            }
        }

        ~Page() {
            LOG_DEBUG("Freeing " << page_);
            free(page_);
        }

        void* get() {
            return page_;
        }

        ClusterLocation& operator[](const size_t in) {
            if (RangedPageAccess) {
                if (in < TCBTMetaDataPageTraits::pageEntries_) {
                    return page_[in];
                } else {
                    throw TCBTMetaDataStoreSimException(
                                                        "Page::operator[]: Index too large");
                }
            } else {
                return page_[in];
            }
        }

        DECLARE_LOGGER("Page")
        ;

        const ClusterLocation& operator[](const size_t in) const {
            if (RangedPageAccess) {

                if (in < TCBTMetaDataPageTraits::pageEntries_) {
                    return page_[in];
                } else {
                    throw TCBTMetaDataStoreSimException(
                                                        "Page::operator[]: Index too large");
                }
            } else {
                return page_[in];
            }
        }

        bool dirty_;
        uint32_t numTimesAccessed_;
        void debugPrint() {
            std::stringstream ss;
            ss << "page with page_ = %p, dirty = " << dirty_
               << ", numTimesAccessed_ = %d";
            char string[1000];
            snprintf(string, 1000, ss.str().c_str(), page_, numTimesAccessed_);
        }

    private:
        ClusterLocation* page_;

    };

#define HANDLE(x) if(!(x)) throw TCBTMetaDataStoreSimException(database_,__FUNCTION__)
    class CachedTCBT {
	    static const uint32_t maxEntriesDefault_ = 1024 * 1024;
    public:
        DECLARE_LOGGER("CachedTCBT")
        ;

        CachedTCBT(const Path location,
                   const size_t /*maxmemsize*/,
                   const TCBTMetaDataStoreSimConfig* config = 0) :
            database_(0)
            ,numEntries_(0)
		    ,maxEntries_(config ? config->internalCacheSize : 1024 * 1024)
        {
            database_ = tcbdbnew();
            try {
                // Need to move to more recent version of tcdb to use this!
                HANDLE(tcbdbsetcmpfunc(database_,&tccmpint32,NULL));

                HANDLE(tcbdbtune(database_,
                                 config ? config->numberOfMembersInLeafPage : 0 ,
                                 config ? config->numberOfMembersInNonLeafPage : 0,
                                 config ? config->numberOfElementsInBucketArray : 0,
                                 config ? config->recordAlignmentByPowerOfTwo: 3,
                                 config ? config->maximumNumberInFreeBlockByPowerOfTwo : -1,
                                 BDBTLARGE));
                HANDLE(tcbdbsetcache(database_,
                                     config ? config->maximumNumberOfLeafNodesCached : 0,
                                     config ? config->maximumNumberOfNonLeafNodesCached : 0));
                assert(sizeof(ClusterAddress) == 8);
                if(not tcbdbsetcmpfunc(database_,tccmpint64,NULL))
                {
                    LOG_WARN("Could not set the compare function");
                }
                HANDLE(tcbdbopen(database_, location.c_str(), BDBOWRITER|BDBOCREAT|BDBOREADER));
            }

            catch(TCBTMetaDataStoreSimException& e) {
                if(database_) {
                    tcbdbclose(database_);
                    tcbdbdel(database_);

                }
            }
        }

        ~CachedTCBT()
        {
            T_;

            try {
                for(MapIterator_t it = map_.begin(); it != map_.end(); ++it) {
                    Page* p = **it;
                    if(p->dirty_) {
                        int32_t key = it.key();
                        HANDLE(tcbdbput(database_,&key,sizeof(key),p->get(),TCBTMetaDataPageTraits::pageSize_));
                    }
                    delete p;
                }
                tcbdbclose(database_);
                tcbdbdel(database_);
            }
            catch(...) {
                LOG_WARN("Caught an error while closing down the CachedTCBT");
                tcbdbclose(database_);
                tcbdbdel(database_);
            }
        }

        void
        reset()
        {
            bool res = tcbdbvanish(database_);
            if(not res)
            {
                LOG_ERROR("Could not reset the database");
                throw fungi::IOException("Could not reset the database");
            }
            while(map_.begin() != map_.end())
            {
                typename Map_t::Iterator i = map_.begin();
                delete **i;
                map_.remove(i);
            }

        }


        void
        getPage(const ClusterAddress i_addr,
                ClusterLocation& loc,
                bool for_write = false)
        {
            T_;

            uint32_t pageAddress = TCBTMetaDataPageTraits::getPageAddress(i_addr);
            MapIterator_t it = map_.getAndPutAtTail(pageAddress);
            Page* p = 0;
            if(it != map_.end())
            {
                p = **it;
                (p->numTimesAccessed_)++;
                if(for_write) {
                    (*p)[TCBTMetaDataPageTraits::getPageOffset(i_addr)] = loc;
                    p->dirty_ = true;
                }
                else
                {
                    loc = (*p)[TCBTMetaDataPageTraits::getPageOffset(i_addr)];
                }

            }
            else
            {
                int32_t numbits;

                void* ret = tcbdbget(database_,
                                     &pageAddress,
                                     sizeof(pageAddress),
                                     &numbits);
                if((not ret) and
                   (not for_write))
                {
                    loc = ClusterLocation::sentinel();
                    return;
                }
                if(ret)
                {

                    assert((uint32_t)numbits==TCBTMetaDataPageTraits::pageSize_);
                }
                p = new Page(ret);
                if(for_write) {
                    (*p)[TCBTMetaDataPageTraits::getPageOffset(i_addr)] = loc;
                    (p->numTimesAccessed_)++;
                    p->dirty_ = true;
                }
                else
                {
                    loc = (*p)[TCBTMetaDataPageTraits::getPageOffset(i_addr)];
                    (p->numTimesAccessed_)++;
                }
                add(i_addr,
                    p);
            }
        }

        void sync()
        {
            T_;
            moveDirtyPagesToTC();
            HANDLE(tcbdbsync(database_));
        }

        void read(const ClusterAddress i_addr,
                  ClusterLocation& location)
        {
            T_;
            getPage(i_addr,location,false);
        }

        void write(const ClusterAddress i_addr,
                   const ClusterLocation& location)
        {
            T_;
            getPage(i_addr, const_cast<ClusterLocation&>(location), true);
        }

        void moveDirtyPagesToTC()
        {
            T_;
            try {
                for(MapIterator_t it = map_.begin(); it != map_.end(); ++it) {
                    Page* p = **it;
                    if(p->dirty_) {
                        int32_t key = it.key();
                        HANDLE(tcbdbput(database_,&key,sizeof(key),p->get(),TCBTMetaDataPageTraits::pageSize_));
                    }
                    p->dirty_ = false;
                }
            }
            catch(...) {
                LOG_WARN("Caught an error moving dirty pages to TC");
                throw;
            }
        }

        void add(const ClusterAddress addr,
                 Page* page)
        {
            T_;
            if(numEntries_ < maxEntries_)
            {
                map_.pushBack(TCBTMetaDataPageTraits::getPageAddress(addr),
                              page);
                numEntries_++;
            }
            else
            {
                MapIterator_t it = map_.begin();

                if(it != map_.end())
                {
                    Page* p = **it;

                    if(p->dirty_)
                    {
                        uint32_t key = it.key();
                        tcbdbput(database_,
                                 &key,
                                 sizeof(key),p->get(),
                                 TCBTMetaDataPageTraits::pageSize_);
                    }
                    delete p;
                    map_.remove(it);
                    map_.pushBack(TCBTMetaDataPageTraits::getPageAddress(addr),
                                  page);
                }
                else
                {
                    uint32_t key = TCBTMetaDataPageTraits::getPageAddress(addr);
                    tcbdbput(database_,&key,sizeof(key),page->get(),TCBTMetaDataPageTraits::pageSize_);
                    delete page;
                }

            }
        }
        typedef fungi::ListContainingMap<uint32_t, Page*> Map_t;
        typedef typename Map_t::Iterator MapIterator_t;

        Map_t map_;
        TCBDB* database_;
        uint32_t numEntries_;
        const uint32_t maxEntries_;

        void debugPrint()
        {
            T_;

            for(MapIterator_t it = map_.begin(); it != map_.end(); ++it) {
                // fudeb("Map Entry key %d with page* %p", it.key(), **it);
                (**it)->debugPrint();
            }
        }

    };
#undef HANDLE
};

template<int pageBits,
         bool RangedAccess = true,
         bool cacheDebugging = true>
class TCBTMetaDataStoreSim : public MetaDataStoreInterface
{
public:
    typedef TCBTMetaDataPageTraits<pageBits, RangedAccess, cacheDebugging> PT;
    //typedef TCBTMetaDataStoreSimConfig MetaDataStoreConfig;
private:
    typedef typename PT::Page Page;

    typedef typename PT::CachedTCBT CachedTC;
public:
    TCBTMetaDataStoreSim(const Path location,
                         const size_t maxmemsize,
                         const TCBTMetaDataStoreSimConfig* config = 0)
        : MetaDataStoreInterface(location,
                                 maxmemsize),
          cachedTC_(getLocation()+"mdstore.tc",
                    maxmemsize,
                    config),
          m_("MetaDataStore"),
          clusterWriteCount_(0)
    {
    }

    ~TCBTMetaDataStoreSim()
    {
    }

    virtual void
    reset()
    {
        cachedTC_.reset();
    }

    virtual void processTLog(TLogReader &)
    {
        // Don't care
    }

    virtual void
    processCloneTLogs(CloneTLogs &)
    {
        // Don't care
    }

    virtual void readCluster(const ClusterAddress i_addr,
                             ClusterLocation& location)
    {
        T_;
        fungi::ScopedLock l(m_);
        cachedTC_.read(i_addr,
                       location);
    }

    virtual void writeCluster(const ClusterAddress i_addr,
                              const ClusterLocation& location)

    {
        //MeasureScopeSCOName ms("MD:writeCluster", location.getSCOName());
        fungi::ScopedLock l(m_);
        cachedTC_.write(i_addr,
                        location);
        clusterWriteCount_ = (++clusterWriteCount_) % VOLUME_DRIVER_WRITE_SYNC_COUNT;
        if (clusterWriteCount_ == 0) {
            cachedTC_.sync();
        }
    }


    virtual void
    sync()
    {
        fungi::ScopedLock l(m_);
        cachedTC_.sync();
    }

    virtual void OnlySyncTC()
    {
        // Y42 is called without lock on the restart failover when that changes tlogs.
        //  m_.assertLocked();
        cachedTC_.sync();
    }

    // TODO: probably pass in data to connect to the director.
    virtual void destroy(bool deleteLocal,
                         bool deleteBackend)
    {
        LOG_INFO("destroying MetaDataStore " << getLocation());
        fungi::ScopedLock l(m_);

        // When we get here
        // 1) No scrubbing for this volume is running
        // 2) No clone for this volume exists.
        // If these preconditions change we're f*cked.

        // TODO tlogManager_.destroy();

        // TODO TC is still open, we can't close it because it's
        // tight to the destructor. luckily our filesystem
        // lets us get away with deleting stuff thats still open...
        if(deleteLocal)
        {
            fungi::File::unlink(getLocation()+"mdstore.tc");

            try
            {
                fungi::File::rmdir(getLocation());
            }
            catch (fungi::IOException &e)
            {
                LOG_WARN("ignoring " << e.what() << " (probably shared with DataStore)");
            }
        }
        if(deleteBackend)
        {
        }
    }
    // } editor and visuals sugest an extra bracket here... ???

private:

    CachedTC cachedTC_;
    //    SnapshotManagement& snapshotManagement_;
    fungi::Mutex m_;
    int clusterWriteCount_;

    DECLARE_LOGGER("MetaDataStore");

    TCBTMetaDataStoreSim(const TCBTMetaDataStoreSim&);
    TCBTMetaDataStoreSim& operator=(const TCBTMetaDataStoreSim&);

};

}
#endif // HELL_FREEZES_OVER
#endif // TCBTMETADATASTORESIM_H_

// Local Variables: **
// mode: c++ **
// End: **
