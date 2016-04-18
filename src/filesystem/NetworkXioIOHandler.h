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

#ifndef __NETWORK_XIO_IO_HANDLER_H_
#define __NETWORK_XIO_IO_HANDLER_H_

#include "FileSystem.h"

#include "NetworkXioWorkQueue.h"
#include "NetworkXioRequest.h"

namespace volumedriverfs
{

class NetworkXioIOHandler
{
public:
    NetworkXioIOHandler(FileSystem& fs,
                        NetworkXioWorkQueuePtr wq)
    : fs_(fs)
    , wq_(wq)
    {}

    ~NetworkXioIOHandler()
    {
        if (handle_)
        {
            fs_.release(std::move(handle_));
        }
    }

    NetworkXioIOHandler(const NetworkXioIOHandler&) = delete;

    NetworkXioIOHandler&
    operator=(const NetworkXioIOHandler&) = delete;

    void
    process_request(Work *work);
    void
    handle_request(NetworkXioRequest* req);

private:
    void handle_open(NetworkXioRequest *req,
                     const std::string& volume_name);

    void handle_close(NetworkXioRequest *req);

    void handle_read(NetworkXioRequest *req,
                     size_t size,
                     uint64_t offset);

    void handle_write(NetworkXioRequest *req,
                      size_t size,
                      uint64_t offset);

    void handle_flush(NetworkXioRequest *req);

    void handle_error(NetworkXioRequest *req,
                      int errval);

private:
    DECLARE_LOGGER("NetworkXioIOHandler");

    FileSystem& fs_;
    NetworkXioWorkQueuePtr wq_;

    std::string volume_name_;
    Handle::Ptr handle_;
};

typedef std::unique_ptr<NetworkXioIOHandler> NetworkXioIOHandlerPtr;

} //namespace volumedriverfs

#endif
