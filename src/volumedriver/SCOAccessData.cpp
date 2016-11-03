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

#include "BackendTasks.h"
#include "ClusterLocation.h"
#include "SCOAccessData.h"

#include <cmath>

#include <boost/filesystem/fstream.hpp>

#include <youtils/FileUtils.h>
#include <backend/BackendException.h>

namespace volumedriver
{

namespace be = backend;

SCOAccessData::SCOAccessData(const backend::Namespace& nspace,
                             float read_activity)
    : nspace_(new backend::Namespace(nspace))
    , read_activity_(read_activity)
{}

SCOAccessData::SCOAccessData(float read_activity)
    : read_activity_(read_activity)
{}


void
SCOAccessData::reserve(size_t reserve)
{
    VERIFY(scos_.empty()); // too strict a check?
    scos_.reserve(reserve);
}

void
SCOAccessData::addData(SCO name,
                       float data)
{
    VERIFY(not std::isinf(data) and not std::isnan(data));
    VERIFY(data >= 0);

    scos_.push_back(std::make_pair(name, data));
}

const SCOAccessData::VectorType&
SCOAccessData::getVector() const
{
    return scos_;
}


float
SCOAccessData::read_activity() const
{
    return read_activity_;
}

const Namespace&
SCOAccessData::getNamespace() const
{
    VERIFY(nspace_);
    return *nspace_;
}

void
SCOAccessData::rebase(const Namespace& nspace)
{
    nspace_.reset(new backend::Namespace(nspace));
    for(VectorType::iterator it = scos_.begin();
        it != scos_.end();
        it++)
    {
        ClusterLocation l(it->first, 0);
        l.incrementCloneID();
        it->first = l.sco();
    }
}

const char*
SCOAccessDataPersistor::backend_name = "sco_access_data";

SCOAccessDataPersistor::SCOAccessDataPersistor(BackendInterfacePtr bi)
    : bi_(std::move(bi))
{
    VERIFY(bi_.get() != 0);
}

SCOAccessDataPtr
SCOAccessDataPersistor::deserialize(const fs::path& p)
{
     SCOAccessDataPtr sad(new SCOAccessData());
     deserialize(p, *sad);
     return sad;
}

void
SCOAccessDataPersistor::deserialize(const fs::path& p,
                                    SCOAccessData& sad)
{
    fs::ifstream ifs(p);
    iarchive_type ia(ifs);
    ia & sad;
    ifs.close();
}

void
SCOAccessDataPersistor::serialize(const fs::path& p,
                                  const SCOAccessData& sad)
{
    Serialization::serializeAndFlush<oarchive_type>(p, sad);
}

void
SCOAccessDataPersistor::pull(SCOAccessData& sad,
                             bool must_exist)
{
    const fs::path p(FileUtils::create_temp_file_in_temp_dir("sco_access_data_for_" +
                                                             bi_->getNS().str()));
    ALWAYS_CLEANUP_FILE(p);

    // TRACE?
    LOG_DEBUG("fetching " << backend_name << " from backend namespace " <<
              bi_->getNS() << " to " << p);

    try
    {
        bi_->read(p.string(),
                  backend_name,
                  InsistOnLatestVersion::T);
        deserialize(p, sad);
    }
    catch (backend::BackendObjectDoesNotExistException& e)
    {
        if (must_exist)
        {
            LOG_ERROR("SCO access data for " << bi_->getNS() << " does not exist");
            throw;
        }
        else
        {
            LOG_INFO("SCO access data for " << bi_->getNS() <<
                     " not found - proceeding with empty SCO access data");
        }
    }

    if (sad.getNamespace() != bi_->getNS())
    {
        LOG_WARN("SCO access data namespace " << sad.getNamespace() <<
                 " doesn't match expectations " << bi_->getNS());
    }
}

SCOAccessDataPtr
SCOAccessDataPersistor::pull(bool must_exist)
{
    SCOAccessDataPtr sad(new SCOAccessData(bi_->getNS()));
    pull(*sad, must_exist);
    return sad;
}

void
SCOAccessDataPersistor::push(const SCOAccessData& sad,
                             const boost::shared_ptr<be::Condition>& cond)
{
    VERIFY(sad.getNamespace() == bi_->getNS());

    const fs::path p(FileUtils::create_temp_file_in_temp_dir("sco_access_data_for_" +
                                                             bi_->getNS().str()));
    ALWAYS_CLEANUP_FILE(p);

    serialize(p, sad);

    // TRACE?
    LOG_DEBUG("writing " << backend_name << " from " << p << " to backend namespace " << bi_->getNS());

    bi_->write(p.string(),
               backend_name,
               OverwriteObject::T,
               nullptr,
               cond);
}

}

// Local Variables: **
// End: **
