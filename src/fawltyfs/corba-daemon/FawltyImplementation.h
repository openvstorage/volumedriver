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

#ifndef FAWLTY_IMPLEMENTATION_H
#define FAWLTY_IMPLEMENTATION_H
#include "fawlty.hh"

class FawltyImplementation
{
public:
    FawltyImplementation(bool& stop_,
                         PortableServer::POA_ptr poa)
        : poa_(poa)
        , stop_(stop_)
    {}


    ~FawltyImplementation()
    {}

    char* ping(const char* string);

    void
    shutdown(bool wait_for_completion);

    Fawlty::FileSystem_ptr
    create(const char* backend_path,
           const char* frontend_path,
           const char* filesystem_name);

    void
    release(Fawlty::FileSystem_ptr p);

private:
    PortableServer::POA_ptr poa_;
    bool& stop_;
};

#endif
