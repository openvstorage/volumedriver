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

#ifndef MDS_WEAK_DATABASE_H_
#define MDS_WEAK_DATABASE_H_

#include "Interface.h"
#include "WeakTable.h"

namespace metadata_server
{

class WeakDataBase
    : public DataBaseInterface
{
public:
    explicit WeakDataBase(DataBaseInterfacePtr db)
        : db_(db)
    {}

    virtual ~WeakDataBase() = default;

    WeakDataBase(const WeakDataBase&) = default;

    WeakDataBase&
    operator=(const WeakDataBase&) = default;

    virtual TableInterfacePtr
    open(const std::string& nspace) override final
    {
        return std::make_shared<WeakTable>(lock_()->open(nspace));
    }

    virtual void
    drop(const std::string& nspace) override final
    {
        return lock_()->drop(nspace);
    }

    virtual std::vector<std::string>
    list_namespaces() override final
    {
        return lock_()->list_namespaces();
    }

    MAKE_EXCEPTION(Exception,
                   fungi::IOException);

private:
    DECLARE_LOGGER("WeakDataBase");

    std::weak_ptr<DataBaseInterface> db_;

    std::shared_ptr<DataBaseInterface>
    lock_() const
    {
        std::shared_ptr<DataBaseInterface> db(db_.lock());
        if (db == nullptr)
        {
            LOG_ERROR("Underlying DataBaseInterface is gone");
            throw Exception("Underlying DataBaseInterface is gone");
        }

        return db;
    }
};

}

#endif // !MDS_WEAK_DATABASE_H_
