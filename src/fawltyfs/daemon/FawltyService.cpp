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

#include "FawltyService.h"
#include "../FileSystem.h"

template<typename Class>
struct Wrapper
{
    Wrapper(const filesystem_handle_t t)
        : pointer_(reinterpret_cast<Class*>(t))
    {}
    template<typename ret_type,
             typename...Args> using mem_fun = ret_type (Class::*)(Args...);

    template<typename ret_type,
             typename... Args>
    inline ret_type
    operator()(mem_fun<ret_type,Args...> f, Args... args)
    {
        return (pointer_->*f)(args...);

    }
    Class* pointer_;
};
cd
typedef Wrapper<fawltyfs::FileSystem> FileSystemWrapper;


static_assert(sizeof(filesystem_handle_t) == sizeof(fawltyfs::FileSystem*),"wrong pointer sizes");

FawltyService::FawltyService()
{}

filesystem_handle_t
FawltyService::make_file_system(const backend_path_t& backend_path,
                                const frontend_path_t& frontend_path,
                                const fuse_args_t& fuse_args,
                                const filesystem_name_t& filesystem_name)
{
    std::vector<std::string> args(fuse_args.begin(), fuse_args.end());

    printf("make_file_system\n");
    return reinterpret_cast<filesystem_handle_t>(new fawltyfs::FileSystem(backend_path,
                                                                          frontend_path,
                                                                          args,
                                                                          filesystem_name.c_str()));

}

void
FawltyService::mount(const filesystem_handle_t filesystem_handle)
{
    (FileSystemWrapper(filesystem_handle))(&fawltyfs::FileSystem::mount);
}

void
FawltyService::umount(const filesystem_handle_t filesystem_handle)
{
    (FileSystemWrapper(filesystem_handle))(&fawltyfs::FileSystem::umount);
}

int32_t
FawltyService::ping()
{
    return 0;
}

void
FawltyService::stop()
{

    VERIFY(server_);

    server_->stop();
}
