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
