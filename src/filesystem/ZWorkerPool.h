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

#ifndef VFS_ZWORKER_POOL_H_
#define VFS_ZWORKER_POOL_H_

#include <deque>
#include <future>
#include <map>

#include <boost/thread.hpp>

#include <cppzmq/zmq.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/Logging.h>

// Design:
// * ZWorkerPool binds to a public address
// * internally it will dispatch requests to that address to a pool of worker threads
//   using a ZMQ router socket
// * this pool is elastic, i.e. it will spawn off a new thread (up to max_workers) if
//   all other threads are busy; these extra threads are then terminated again after doing
//   their work (only min_workers are kept alive)
// * requests are not dispatched round-robin to the workers (as a DEALER would do, but
//   that doesn't work nicely with newly added workers, cf.
//   ZMQTest.elastic_worker_pool_based_on_dealer) but are directed to a specific worker
//   from the set of unused ones
// * a worker is considered busy once a request has been sent off to to it until a
//   response from it is seen or until it (re)registers
//
// With that we have:
// * a front(end) ROUTER socket: recv requests from clients, forward responses
//   to clients
// * a back(end) ROUTER socket: forward requests from clients, recv responses from
//   workers
// . These sockets are not class members to prevent access from multiple threads, but are
// owned by the aforementioned internal router thread.
//
// Workers are pretty simple creatures built around a REQ socket:
// * on startup they register with the pool (by sending an empty request)
// * client requests are mapped to responses to the REQ socket, (client) responses are
//   sent back as a new request from the REQ socket
// * if the pool sends an empty reply the worker exits
//
// Workers are identified by a ZWorkerId which is allocated by the pool to ensure that
// these identifies are not reused (cf. comment in next_zworker_id_()).
//
// The pool is torn down when the ZMQ context is terminated.
//
// TODO: push to youtils?

BOOLEAN_ENUM(CanMakeProgress);

namespace volumedriverfs
{

struct ZWorker;

class ZWorkerPool
{
public:
    typedef std::vector<zmq::message_t> MessageParts;
    typedef std::function<MessageParts(MessageParts&&)> WorkerFun;

    ZWorkerPool(const std::string& name,
                zmq::context_t& ztx,
                const std::string& pub_addr,
                WorkerFun&& worker_fun,
                uint16_t min_workers,
                uint16_t max_workers = 128);

    ~ZWorkerPool();

    ZWorkerPool(const ZWorkerPool&) = delete;

    ZWorkerPool&
    operator=(const ZWorkerPool&) = delete;

    size_t
    size() const
    {
        boost::lock_guard<decltype(lock_)> g(lock_);
        return unused_ones_.size() + busy_ones_.size();
    }

    void
    resize(uint16_t min, uint16_t max);

    void
    validate_settings(uint16_t min, uint16_t max);

    typedef uint64_t ZWorkerId;

private:
    typedef std::unique_ptr<ZWorker> ZWorkerPtr;

    DECLARE_LOGGER("ZWorkerPool");

    uint16_t min_;
    uint16_t max_;
    WorkerFun worker_fun_;
    zmq::context_t& ztx_;
    ZWorkerId next_id_ = 1;

    // We could ditch the lock if we ditch size() - every other access to unused_ones_
    // and busy_ones_ happens in the router_thread_
    mutable boost::mutex lock_;

    std::deque<ZWorkerPtr> unused_ones_;
    std::map<ZWorkerId, ZWorkerPtr> busy_ones_;

    const std::string name_;
    const std::string pub_addr_;

    boost::thread router_thread_;

    std::promise<bool> initialized_;

    std::string
    back_address_() const;

    void
    route_();

    CanMakeProgress
    handle_front_(zmq::socket_t& front_sock,
                  zmq::socket_t& back_sock);

    void
    handle_back_(zmq::socket_t& front_sock,
                 zmq::socket_t& back_sock);

    void
    worker_ready_(ZWorkerId id);

    void
    worker_done_(ZWorkerId id,
                 zmq::socket_t& back_sock);

    void
    stop_worker_(ZWorkerId id,
                 zmq::socket_t& back_sock);

    ZWorkerId
    next_zworker_id_();
};

}

#endif // !VFS_ZWORKER_POOL_H_
