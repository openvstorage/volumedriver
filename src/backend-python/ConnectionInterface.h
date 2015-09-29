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

#ifndef PYTHON_BACKEND_CONNECTION_INTERFACE_H
#define PYTHON_BACKEND_CONNECTION_INTERFACE_H

#include <boost/filesystem.hpp>
#include <boost/python/list.hpp>

#include <youtils/CheckSum.h>

#include <backend/BackendConnectionInterface.h>
#include <backend/BackendConnectionManager.h>

namespace pythonbackend
{

class ConnectionInterface
{
public:
    ConnectionInterface() = delete;

    TODO("Pass the manager here too");

    TODO("Have an info call that completely describes the connection");

    ConnectionInterface(backend::BackendConnectionInterfacePtr&& ptr)
        : conn_(std::move(ptr))
    {
        TODO("set the timeout in some way")
    }

    boost::python::list
    listNamespaces()
    {
        std::list<std::string> namespaces;
        conn_->listNamespaces(namespaces);
        boost::python::list lst;
        for(const std::string& s : namespaces)
        {
            lst.append(s);
        }
        return lst;
    }


    boost::python::list
    listObjects(const std::string& nspace)
    {
        std::list<std::string> objects;

        conn_->listObjects(backend::Namespace(nspace),
                           objects);

        boost::python::list lst;

        for (const std::string& s : objects)
        {
            lst.append(s);
        }
        return lst;

    }

    bool
    namespaceExists(const std::string& nspace)
    {
        return conn_->namespaceExists(backend::Namespace(nspace));
    }

    void
    createNamespace(const std::string& nspace)
    {
        conn_->createNamespace(backend::Namespace(nspace));
    }

    void
    deleteNamespace(const std::string& nspace)
    {
        conn_->deleteNamespace(backend::Namespace(nspace));
    }

    void
    read(const std::string& nspace,
         const std::string& destination,
         const std::string& name,
         bool insist_on_latest_version)
    {
        conn_->read(backend::Namespace(nspace),
                    destination,
                    name,
                    insist_on_latest_version ?
                    InsistOnLatestVersion::T :
                    InsistOnLatestVersion::F);
    }

    void
    write(const std::string& nspace,
          const std::string& location,
          const std::string& name,
          const OverwriteObject overwrite = OverwriteObject::F)
    {
        conn_->write(backend::Namespace(nspace),
                     location,
                     name,
                     overwrite,
                     0);
    }

    bool
    objectExists(const std::string& nspace,
                 const std::string& name)
    {
        return conn_->objectExists(backend::Namespace(nspace),
                                   name);
    }

    void
    remove(const std::string& nspace,
           const std::string& name,
           const bool may_not_exist = false)
    {
        return conn_->remove(backend::Namespace(nspace),
                             name,
                             may_not_exist ?
                             ObjectMayNotExist::T :
                             ObjectMayNotExist::F);
    }

    uint64_t
    getSize(const std::string& nspace,
            const std::string& name)
    {
        return conn_->getSize(backend::Namespace(nspace),
                              name);
    }


    std::string
    str() const
    {
        using namespace std::string_literals;
        return "<ConnectionInterface>"s;
    }

private:
    backend::BackendConnectionInterfacePtr conn_;
};
}

#endif // PYTHON_BACKEND_CONNECTION_INTERFACE_H

// Local Variables: **
// mode: c++ **
// End: **
