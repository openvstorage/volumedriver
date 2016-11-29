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

#include "FileSystemMetaDataClient.h"
#include "IntegerConverter.h"
#include "StringyConverter.h"

#include <boost/make_shared.hpp>
#include <boost/python/class.hpp>
#include <boost/python/copy_const_reference.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/register_ptr_to_python.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/LockedArakoon.h>
#include <youtils/Logger.h>

#include <filesystem/DirectoryEntry.h>
#include <filesystem/FrontendPath.h>
#include <filesystem/MetaDataStore.h>

namespace volumedriverfs
{

namespace python
{

namespace ara = arakoon;
namespace bpy = boost::python;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

TODO("AR: expose LockedArakoon instead");

using MetaDataStorePtr = boost::shared_ptr<MetaDataStore>;

MetaDataStorePtr
make_metadata_store(const ClusterId& cluster_id,
                    const ara::ClusterID& ara_cluster_id,
                    const std::vector<ara::ArakoonNodeConfig>& ara_node_configs)
{
    auto larakoon(std::make_shared<yt::LockedArakoon>(ara_cluster_id,
                                                      ara_node_configs));
    return boost::make_shared<MetaDataStore>(larakoon,
                                             cluster_id,
                                             UseCache::F);
}

void
add_directory_entry(MetaDataStore* mdstore,
                    const ObjectId& parent_id,
                    const ObjectId& object_id,
                    const std::string& name,
                    const DirectoryEntry::Type typ,
                    const Permissions perms,
                    const UserId user_id,
                    const GroupId group_id)
{
    auto dentry(boost::make_shared<DirectoryEntry>(object_id,
                                                   typ,
                                                   mdstore->alloc_inode(),
                                                   perms,
                                                   user_id,
                                                   group_id));

    mdstore->add(parent_id,
                 name,
                 dentry);
}

}

void
FileSystemMetaDataClient::registerize()
{
    REGISTER_STRINGY_CONVERTER(FrontendPath);
    REGISTER_INTEGER_CONVERTER(Inode);
    REGISTER_INTEGER_CONVERTER(Permissions);
    REGISTER_INTEGER_CONVERTER(GroupId);
    REGISTER_INTEGER_CONVERTER(UserId);

    bpy::enum_<DirectoryEntry::Type>("DirectoryEntryType")
        .value("DIRECTORY", DirectoryEntry::Type::Directory)
        .value("FILE", DirectoryEntry::Type::File)
        .value("VOLUME", DirectoryEntry::Type::Volume)
        ;

    GroupId (DirectoryEntry::*get_group_id)() const =
        &DirectoryEntry::group_id;

    UserId (DirectoryEntry::*get_user_id)() const =
        &DirectoryEntry::user_id;

    Permissions (DirectoryEntry::*get_permissions)() const =
        &DirectoryEntry::permissions;

    bpy::class_<DirectoryEntry,
                boost::noncopyable>("DirectoryEntry",
                                    "A volumedriverfs directory entry",
                                    bpy::no_init)
        .def("type",
             &DirectoryEntry::type,
             "Query the DirectoryEntryType\n"
             "@return: DirectoryEntryType\n")
        .def("object_id",
             &DirectoryEntry::object_id,
             bpy::return_value_policy<bpy::copy_const_reference>(),
             "Retrieve the ObjectId\n"
             "@return: string, ObjectId\n")
        .def("inode",
             &DirectoryEntry::inode,
             "Retrieve the Inode\n"
             "@return: integer, Inode\n")
        .def("group_id",
             get_group_id,
             "Retrieve the GroupId\n"
             "@return: integer, GroupId\n")
        .def("user_id",
             get_user_id,
             "Retrieve the UserId\n"
             "@return: integer, UserId\n")
        .def("permissions",
             get_permissions,
             "Retrieve the Permissions\n"
             "@return: integer, Permission\n")
        ;

    bpy::register_ptr_to_python<DirectoryEntryPtr>();

    DirectoryEntryPtr (MetaDataStore::*find_path)(const FrontendPath&)
        = &MetaDataStore::find;

    DirectoryEntryPtr (MetaDataStore::*find_id)(const ObjectId&)
        = &MetaDataStore::find;

    std::list<std::string> (MetaDataStore::*list_path)(const FrontendPath&)
        = &MetaDataStore::list;

    std::list<std::string> (MetaDataStore::*list_id)(const ObjectId&)
        = &MetaDataStore::list;

    void (MetaDataStore::*unlink_id)(const ObjectId&,
                                     const std::string&)
        = &MetaDataStore::unlink;

    DirectoryEntryPtr (MetaDataStore::*rename_id)(const ObjectId&,
                                                  const std::string&,
                                                  const ObjectId&,
                                                  const std::string&)
        = &MetaDataStore::rename;

    bpy::class_<MetaDataStore,
                MetaDataStorePtr,
                boost::noncopyable>("FileSystemMetaDataClient",
                                    "Access to the filesystem's metadata",
                                    bpy::no_init)
        .def("__init__",
             bpy::make_constructor(make_metadata_store,
                                   bpy::default_call_policies(),
                                   (bpy::args("cluster_id"),
                                    bpy::args("arakoon_cluster_id"),
                                    bpy::args("arakoon_node_configs"))),
             "Construct a FileSystemMetaDataClient\n"
             "@param cluster_id: string, filesystem cluster ID\n"
             "@param arakoon_cluster_id: string, Arakoon cluster ID\n"
             "@param arakoon_node_configs: list of ArakoonNodeConfigs\n")
        .def("list_path",
             list_path,
             bpy::args("path"),
             "list the names of all children of a given path\n"
             "@param path, string, path of parent\n"
             "@return list of strings\n")
        .def("list",
             list_id,
             bpy::args("object_id"),
             "list the names of all children of a given ObjectId\n"
             "@param object_id, string, ObjectId\n"
             "@return list of strings\n")
        .def("find_path",
             find_path,
             bpy::args("path"),
             "get the DirectoryEntry of a given path\n"
             "@param path, string\n"
             "@return DirectoryEntry\n")
        .def("find_id",
             find_id,
             bpy::args("object_id"),
             "get the DirectoryEntry of a given ObjectId\n"
             "@param object_id, string, ObjectId\n"
             "@return DirectoryEntry\n")
        .def("lookup",
             &MetaDataStore::find_path,
             bpy::args("object_id"),
             "look up the path of a DirectoryEntry with a given ID\n"
             "@param object_id, string, ObjectId\n"
             "@return string\n")
        .def("mknod",
             add_directory_entry,
             (bpy::args("parent_id"),
              bpy::args("object_id"),
              bpy::args("name"),
              bpy::args("type"),
              bpy::args("permissions"),
              bpy::args("user_id"),
              bpy::args("group_id")),
             "Add a new DirectoryEntry\n"
             "@param parent_id, string, ObjectID of parent entry\n"
             "@param object_id, string, ObjectID of the new entry\n"
             "@param name, string, name of the new entry\n"
             "@param type, DirectoryEntryType, type of entry\n"
             "@param permissions, integer, access permissions of the entry\n"
             "@param user_id, integer, Owner ID of the entry\n"
             "@param group_id, integer, Group ID of the entry\n"
             "@return nothing")
        .def("unlink",
             unlink_id,
             (bpy::args("parent_id"),
              bpy::args("name")),
             "Remove a directory entry\n"
             "@param parent_id, string, ObjectID of parent entry\n"
             "@param name, string, name of the entry to remove\n"
             "@return nothing")
        .def("rename",
             rename_id,
             (bpy::args("from_parent"),
              bpy::args("from"),
              bpy::args("to_parent"),
              bpy::args("to")),
             "Rename a directory entry\n"
             "@param from_parent, string, ObjectID of current parent entry\n"
             "@param from, string, current name of the entry\n"
             "@param to_parent, string, ObjectID of new parent entry\n"
             "@param to, string, new name of the entry\n"
             "@return old DirectoryEntry behind \"to\", if any\n")
        ;
}

}

}
