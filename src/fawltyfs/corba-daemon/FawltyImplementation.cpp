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

#include "FawltyImplementation.h"
#include "CorbaFileSystem.h"
char* FawltyImplementation::ping(const char* mesg)
{
    return CORBA::string_dup(mesg);
}

void
FawltyImplementation::shutdown(bool /*wait_for_completion*/)
{
    stop_ = true;
}


Fawlty::FileSystem_ptr
FawltyImplementation::create(const char* backend_path,
                             const char* frontend_path,
                             const char* filesystem_name)
{

    POA_Fawlty::FileSystem_tie<CorbaFileSystem>* filesystem =
        new POA_Fawlty::FileSystem_tie<CorbaFileSystem>(new CorbaFileSystem(backend_path,
                                                                            frontend_path,
                                                                            filesystem_name),
                                                        poa_,
                                                        true);
    poa_->activate_object(filesystem);
    return filesystem->_this();
}

void
FawltyImplementation::release(Fawlty::FileSystem_ptr /*p*/)
{
    // CORBA::release(p);
}

