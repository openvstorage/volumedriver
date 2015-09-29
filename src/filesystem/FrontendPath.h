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

#ifndef VFS_FRONTEND_PATH_H_
#define VFS_FRONTEND_PATH_H_

#include <youtils/StrongTypedPath.h>

// we distinguish between externally visible paths (passed in by FUSE) and the
// backend ones (represented by boost::filesystem::path)
STRONG_TYPED_PATH(volumedriverfs, FrontendPath);

#endif // !VFS_FRONTEND_PATH_H_
