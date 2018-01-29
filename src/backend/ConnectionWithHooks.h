// Copyright (C) 2018 iNuron NV
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

#ifndef BACKEND_CONNECTION_WITH_HOOKS_H_
#define BACKEND_CONNECTION_WITH_HOOKS_H_

#include "BackendConnectionInterface.h"

#include <functional>
#include <type_traits>

#include <boost/mpl/map.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace backend
{

enum Operation
{
    ListObjects,
    NamespaceExists,
    CreateNamespace,
    DeleteNamespace,
    ListNamespaces,
    Read,
    ReadTag,
    GetTag,
    Write,
    WriteTag,
    PartialRead,
    ObjectExists,
    Remove,
    GetSize,
    GetCheckSum,
};

template<typename... Args>
struct Hook
{
    static boost::shared_mutex rwlock;
    static std::function<void(Args...)> fun;
};

template<typename... Args>
boost::shared_mutex Hook<Args...>::rwlock;

template<typename... Args>
std::function<void(Args...)> Hook<Args...>::fun;

// A wrapper around BackendConnectionInterface that allows to set *at most* one
// hook (a callable taking the same parameters as the wrapped operation but
// always returning void) per operation that is invoked with the wrapped op's
// args.
// Adding a hook returns a HookHandle which on destruction will remove the hook
// again.
class ConnectionWithHooks
    : public BackendConnectionInterface
{
public:
    explicit ConnectionWithHooks(std::unique_ptr<BackendConnectionInterface> inner)
        : inner_(std::move(inner))
    {}

    ~ConnectionWithHooks() = default;

    bool
    healthy() const final
    {
        return inner_->healthy();
    }

    void
    listObjects_(const Namespace& nspace,
                 std::list<std::string>& objects) final
    {
        wrap_<Operation::ListObjects>(&BackendConnectionInterface::listObjects_,
                                      nspace,
                                      objects);
    }

    bool
    namespaceExists_(const Namespace& nspace) final
    {
        return wrap_<Operation::NamespaceExists>(&BackendConnectionInterface::namespaceExists_,
                                                 nspace);
    }

    void
    createNamespace_(const Namespace& nspace,
                     const NamespaceMustNotExist must_not_exist = NamespaceMustNotExist::T) final
    {
        wrap_<Operation::CreateNamespace>(&BackendConnectionInterface::createNamespace_,
                                          nspace,
                                          must_not_exist);
    }

    void
    deleteNamespace_(const Namespace& nspace) final
    {
        wrap_<Operation::DeleteNamespace>(&BackendConnectionInterface::deleteNamespace_,
                                          nspace);
    }

    void
    listNamespaces_(std::list<std::string>& nspaces) final
    {
        wrap_<Operation::ListNamespaces>(&BackendConnectionInterface::listNamespaces_,
                                         nspaces);
    }

    std::unique_ptr<youtils::UniqueObjectTag>
    get_tag_(const Namespace& nspace,
             const std::string& name) final
    {
        return wrap_<Operation::GetTag>(&BackendConnectionInterface::get_tag_,
                                        nspace,
                                        name);
    }

    void
    write_(const Namespace& nspace,
           const boost::filesystem::path& src,
           const std::string& name,
           const OverwriteObject overwrite,
           const youtils::CheckSum* chksum = nullptr,
           const boost::shared_ptr<Condition>& cond = nullptr) final
    {
        wrap_<Operation::Write>(&BackendConnectionInterface::write_,
                                nspace,
                                src,
                                name,
                                overwrite,
                                chksum,
                                cond);
    }

    std::unique_ptr<youtils::UniqueObjectTag>
    write_tag_(const Namespace& nspace,
               const boost::filesystem::path& src,
               const std::string& name,
               const youtils::UniqueObjectTag* tag,
               const OverwriteObject overwrite) final
    {
        return wrap_<Operation::WriteTag>(&BackendConnectionInterface::write_tag_,
                                          nspace,
                                          src,
                                          name,
                                          tag,
                                          overwrite);
    }

    std::unique_ptr<youtils::UniqueObjectTag>
    read_tag_(const Namespace& nspace,
              const boost::filesystem::path& dst,
              const std::string& name) final
    {
        return wrap_<Operation::ReadTag>(&BackendConnectionInterface::read_tag_,
                                         nspace,
                                         dst,
                                         name);
    }

    bool
    objectExists_(const Namespace& nspace,
                  const std::string& name) final
    {
        return wrap_<Operation::ObjectExists>(&BackendConnectionInterface::objectExists_,
                                              nspace,
                                              name);
    }

    void
    remove_(const Namespace& nspace,
            const std::string& name,
            const ObjectMayNotExist may_not_exist,
            const boost::shared_ptr<Condition>& cond = nullptr) final
    {
        wrap_<Operation::Remove>(&BackendConnectionInterface::remove_,
                                 nspace,
                                 name,
                                 may_not_exist,
                                 cond);
    }

    uint64_t
    getSize_(const Namespace& nspace,
             const std::string& name) final
    {
        return wrap_<Operation::GetSize>(&BackendConnectionInterface::getSize_,
                                         nspace,
                                         name);
    }

    youtils::CheckSum
    getCheckSum_(const Namespace& nspace,
                 const std::string& name) final
    {
        return wrap_<Operation::GetCheckSum>(&BackendConnectionInterface::getCheckSum_,
                                             nspace,
                                             name);
    }

    void
    read_(const Namespace& nspace,
          const boost::filesystem::path& dst,
          const std::string& name,
          InsistOnLatestVersion insist_on_latest) final
    {
        return wrap_<Operation::Read>(&BackendConnectionInterface::read_,
                                      nspace,
                                      dst,
                                      name,
                                      insist_on_latest);
    }

    bool
    partial_read_(const Namespace& nspace,
                  const BackendConnectionInterface::PartialReads& partial_reads,
                  InsistOnLatestVersion insist_on_latest,
                  PartialReadCounter& prc) final
    {
        return wrap_<Operation::PartialRead>(&BackendConnectionInterface::partial_read_,
                                             nspace,
                                             partial_reads,
                                             insist_on_latest,
                                             prc);
    }

    template<Operation op>
    class HookHandle
    {
    public:
        HookHandle(const HookHandle&) = delete;

        HookHandle(HookHandle&& other)
            : owner_(false)
        {
            std::swap(owner_,
                      other.owner_);
        }

        HookHandle&
        operator=(const HookHandle&) = delete;

        HookHandle&
        operator=(HookHandle&& other)
        {
            if (this != &other)
            {
                std::swap(owner_,
                          other.owner_);
            }

            return *this;
        }

        ~HookHandle()
        {
            if (owner_)
            {
                ConnectionWithHooks::remove_hook_<op>();
            }
        }

    private:
        friend class ConnectionWithHooks;

        bool owner_;

        HookHandle()
            : owner_(true)
        {}
    };

    template<Operation op,
             typename Fun,
             typename... Args>
    static HookHandle<op>
    add_hook(Fun fun)
    {
        using K = boost::mpl::int_<op>;
        static_assert(boost::mpl::has_key<HookMap, K>::value,
                      "no hook found for operation");
        using V = typename boost::mpl::at<HookMap, K>::type;

        boost::unique_lock<decltype(V::rwlock)> u(V::rwlock);
        VERIFY(not V::fun);
        V::fun = std::move(fun);

        return HookHandle<op>();
    }

private:
    DECLARE_LOGGER("ConnectionWithHooks");

    std::unique_ptr<BackendConnectionInterface> inner_;

    using ListObjectsKey = boost::mpl::int_<Operation::ListObjects>;
    using ListObjectsVal = Hook<const Namespace&,
                                std::list<std::string>&>;

    using NamespaceExistsKey = boost::mpl::int_<Operation::NamespaceExists>;
    using NamespaceExistsVal = Hook<const Namespace&>;

    using CreateNamespaceKey = boost::mpl::int_<Operation::CreateNamespace>;
    using CreateNamespaceVal = Hook<const Namespace&,
                                    const NamespaceMustNotExist>;

    using DeleteNamespaceKey = boost::mpl::int_<Operation::DeleteNamespace>;
    using DeleteNamespaceVal = Hook<const Namespace&>;

    using ListNamespacesKey = boost::mpl::int_<Operation::ListNamespaces>;
    using ListNamespacesVal = Hook<std::list<std::string>&>;

    using GetTagKey = boost::mpl::int_<Operation::GetTag>;
    using GetTagVal = Hook<const Namespace&,
                           const std::string&>;

    using WriteKey = boost::mpl::int_<Operation::Write>;
    using WriteVal = Hook<const Namespace&,
                          const boost::filesystem::path&,
                          const std::string&,
                          const OverwriteObject,
                          const youtils::CheckSum*,
                          const boost::shared_ptr<Condition>&>;

    using PartialReadKey = boost::mpl::int_<Operation::PartialRead>;
    using PartialReadVal = Hook<const Namespace&,
                                const BackendConnectionInterface::PartialReads&,
                                InsistOnLatestVersion,
                                PartialReadCounter&>;

    using ReadKey = boost::mpl::int_<Operation::Read>;
    using ReadVal = Hook<const Namespace&,
                         const boost::filesystem::path&,
                         const std::string&,
                         InsistOnLatestVersion>;

    using WriteTagKey = boost::mpl::int_<Operation::WriteTag>;
    using WriteTagVal = Hook<const Namespace&,
                             const boost::filesystem::path&,
                             const std::string&,
                             const youtils::UniqueObjectTag*,
                             const OverwriteObject>;

    using ReadTagKey = boost::mpl::int_<Operation::ReadTag>;
    using ReadTagVal = Hook<const Namespace&,
                            const boost::filesystem::path&,
                            const std::string&>;

    using ObjectExistsKey = boost::mpl::int_<Operation::ObjectExists>;
    using ObjectExistsVal = Hook<const Namespace&,
                                 const std::string&>;

    using RemoveKey = boost::mpl::int_<Operation::Remove>;
    using RemoveVal = Hook<const Namespace&,
                           const std::string&,
                           const ObjectMayNotExist,
                           const boost::shared_ptr<Condition>&>;

    using GetSizeKey = boost::mpl::int_<Operation::GetSize>;
    using GetSizeVal = Hook<const Namespace&,
                            const std::string&>;

    using GetCheckSumKey = boost::mpl::int_<Operation::GetCheckSum>;
    using GetCheckSumVal = Hook<const Namespace&,
                                const std::string&>;

    using HookMap = boost::mpl::map<
        boost::mpl::pair<ListObjectsKey, ListObjectsVal>,
        boost::mpl::pair<NamespaceExistsKey, NamespaceExistsVal>,
        boost::mpl::pair<CreateNamespaceKey, CreateNamespaceVal>,
        boost::mpl::pair<DeleteNamespaceKey, DeleteNamespaceVal>,
        boost::mpl::pair<ListNamespacesKey, ListNamespacesVal>,
        boost::mpl::pair<GetTagKey, GetTagVal>,
        boost::mpl::pair<WriteKey, WriteVal>,
        boost::mpl::pair<ReadKey, ReadVal>,
        boost::mpl::pair<ReadTagKey, ReadTagVal>,
        boost::mpl::pair<WriteTagKey, WriteTagVal>,
        boost::mpl::pair<ObjectExistsKey, ObjectExistsVal>,
        boost::mpl::pair<RemoveKey, RemoveVal>,
        boost::mpl::pair<GetSizeKey, GetSizeVal>,
        boost::mpl::pair<GetCheckSumKey, GetCheckSumVal>,
        boost::mpl::pair<PartialReadKey, PartialReadVal>
        >;

    template<Operation op,
             typename Ret,
             typename... InArgs,
             typename... OutArgs>
    Ret
    wrap_(Ret (BackendConnectionInterface::*mem_fn)(OutArgs...),
          InArgs&&... args)
    {
        using K = boost::mpl::int_<op>;
        static_assert(boost::mpl::has_key<HookMap, K>::value,
                      "no hook found for operation");
        using V = typename boost::mpl::at<HookMap, K>::type;

        {
            boost::shared_lock<decltype(V::rwlock)> s(V::rwlock);
            if (V::fun)
            {
                V::fun(args...);
            }
        }

        return (inner_.get()->*mem_fn)(std::forward<InArgs>(args)...);
    }

    template<Operation op>
    static void
    remove_hook_()
    {
        using K = boost::mpl::int_<op>;
        static_assert(boost::mpl::has_key<HookMap, K>::value,
                      "no hook found for operation");
        using V = typename boost::mpl::at<HookMap, K>::type;

        boost::unique_lock<decltype(V::rwlock)> u(V::rwlock);
        V::fun = nullptr;
    }
};

}

#endif // !BACKEND_CONNECTION_WITH_HOOKS_H_
