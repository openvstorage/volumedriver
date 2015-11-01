// Copyright 2015 iNuron NV
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
