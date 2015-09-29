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

#include "ExGTest.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <map>

#include <boost/timer.hpp>

#include <youtils/System.h>

namespace volumedriver
{

typedef std::pair<uint64_t,const std::string*> entry;

class CacheSimInterface {
public:
    virtual ~CacheSimInterface() {};
    virtual void insert(entry &) = 0;
    virtual bool lookup(entry &) = 0;
};

class CacheSim1Map : public CacheSimInterface {
public:
    ~CacheSim1Map()
    {
    }

    virtual void insert(entry &e)
    {
        std::string k;
        makeKey_(e, k);
        map_.insert(std::make_pair(k, true));
    }

    virtual bool lookup(entry &e)
    {
        std::string k;
        makeKey_(e, k);
        return map_.find(k) != map_.end();
    }

private:
    typedef std::map<std::string, bool> map_t;
    map_t map_;

    void makeKey_(const entry &e, std::string &key)
    {
        std::stringstream ss;
        ss << e.first << "/" << *e.second;
        key = ss.str();
    }
};

class CacheSim2Maps : public CacheSimInterface {
public:
    ~CacheSim2Maps()
    {
        for (nsMap_t::iterator i = map_.begin(); i != map_.end(); ++i) {
            scoMap_t *scoMap = i->second;
            scoMap->clear();
        }
    }

    virtual void insert(entry &e)
    {
        nsMap_t::iterator i = map_.find(*e.second);
        if (i == map_.end()) {
            map_.insert(std::make_pair(*e.second, new scoMap_t()));
        }
        map_[*e.second]->insert(std::make_pair(e.first, true));
    }

    virtual bool lookup(entry &e)
    {
        nsMap_t::iterator i = map_.find(*e.second);
        if (i == map_.end()) {
            return false;
        }

        scoMap_t *scoMap = i->second;
        return scoMap->find(e.first) != scoMap->end();
    }

private:
    typedef std::map<uint64_t, bool> scoMap_t;
    typedef std::map<std::string, scoMap_t *> nsMap_t;

    nsMap_t map_;
};

class CacheSim3Maps : public CacheSimInterface {
public:
    ~CacheSim3Maps()
    {
    }

    virtual void insert(entry &e)
    {
        nsMap_t::iterator i = map_.find(*e.second);
        if (i == map_.end()) {
            map_.insert(std::make_pair(*e.second, scoMap_t()));
        }
        map_[*e.second].insert(std::make_pair(e.first, true));
    }

    virtual bool lookup(entry &e)
    {
        nsMap_t::iterator i = map_.find(*e.second);
        if (i == map_.end()) {
            return false;
        }

        scoMap_t &scoMap = i->second;
        return scoMap.find(e.first) != scoMap.end();
    }

private:
    typedef std::map<uint64_t, bool> scoMap_t;
    typedef std::map<std::string, scoMap_t> nsMap_t;

    nsMap_t map_;
};

class Test2TCMaps : public ExGTest
{
public:
    DECLARE_LOGGER("Test2TCMaps");

    Test2TCMaps()
        : numSpaces_(youtils::System::get_env_with_default("NUMBER_OF_SPACES", 5U))
        , entries_(youtils::System::get_env_with_default("NUMBER_OF_TIMES", 1000000U))
    {}

    void SetUp()
    {
        for(unsigned i = 0; i < numSpaces_; ++i)
        {
            std::stringstream str("namespace_");
            str << i;

            namespaces.push_back(new std::string(str.str()));
        }
    }

    entry
    createEntry()
    {
        uint64_t i = rand();
        const std::string* name = namespaces[i%numSpaces_];
        return std::make_pair(i,name);
    }

    void fillCache(CacheSimInterface &cache)
    {
        for (size_t i = 0; i < entries_.size(); ++i)
        {
            entries_[i] = createEntry();
            cache.insert(entries_[i]);
        }
    }

    void doLookups(CacheSimInterface &cache)
    {
        boost::timer i;
        //        i.start();
        for (size_t i = 0; i < entries_.size(); ++i)
        {
            EXPECT_TRUE(cache.lookup(entries_[i]));
        }
        //        i.stop();
        LOG_INFO("lookups: " << i.elapsed());
    }

protected:
    unsigned numSpaces_;
    std::vector<std::string*> namespaces;
    unsigned numTimes_;
    std::vector<entry> entries_;
};

TEST_F(Test2TCMaps, DISABLED_OneBigMapRequiringStringOps)
{
    //std::ofstream o("/tmp/test2tc.1");
    CacheSim1Map map;
    fillCache(map);
    doLookups(map);
}

TEST_F(Test2TCMaps, DISABLED_NSMapContainingPtrsToSCOMaps)
{
    CacheSim2Maps map;
    fillCache(map);
    doLookups(map);
}

TEST_F(Test2TCMaps, DISABLED_NSMapContainingSCOMaps)
{
    CacheSim3Maps map;
    fillCache(map);
    doLookups(map);
}

}

// Local Variables: **
// mode: c++ **
// End: **
