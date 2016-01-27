// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __LIB_OVS_COMMON_H
#define __LIB_OVS_COMMON_H

enum class RequestOp
{
    Read,
    Write,
    Flush,
    AsyncFlush,
};

struct ovs_buffer
{
    void *buf;
    size_t size;
};

struct ovs_completion
{
    ovs_callback_t complete_cb;
    void *cb_arg;
    bool _on_wait;
    bool _calling;
    bool _signaled;
    bool _failed;
    size_t _rv;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
};

struct ovs_aio_request
{
    struct ovs_aiocb *ovs_aiocbp;
    ovs_completion_t *completion;
    RequestOp _op;
    bool _on_suspend;
    bool _canceled;
    bool _completed;
    bool _signaled;
    bool _failed;
    int _errno;
    size_t _rv;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
};

#endif
