// Copyright 2016 iNuron NV
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

#ifndef VFS_FAST_PATH_COOKIE_H_
#define VFS_FAST_PATH_COOKIE_H_

#include <boost/optional.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>

namespace volumedriver
{

class Volume;

}

namespace volumedriverfs
{

// OUR_STRONG_TYPEDEFs would be nice but we ain't got no operator== for weak_ptr.
// No support for local files for now. Look into boost::variant when going there.
struct LocalVolumeCookie
{
    std::weak_ptr<volumedriver::Volume> volume;

    explicit LocalVolumeCookie(std::weak_ptr<volumedriver::Volume> v)
        : volume(v)
    {}
};

using FastPathCookie = std::shared_ptr<LocalVolumeCookie>;

}

#endif // !VFS_FAST_PATH_COOKIE_H_
