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

#ifndef __NETWORK_XIO_CLIENT_H_
#define __NETWORK_XIO_CLIENT_H_

#include <libxio.h>

#include <queue>
#include <boost/thread/lock_guard.hpp>
#include <youtils/SpinLock.h>

#include <youtils/Logger.h>

#include "NetworkXioProtocol.h"

namespace volumedriverfs
{

extern void ovs_xio_aio_complete_request(void *request,
                                         ssize_t retval,
                                         int errval);

extern void ovs_xio_complete_request_control(void *request,
                                             ssize_t retval,
                                             int errval);

class NetworkXioClient
{
public:
    NetworkXioClient(const std::string& uri);

    ~NetworkXioClient();

    struct xio_msg_s
    {
        xio_msg xreq;
        const void *opaque;
        NetworkXioMsg msg;
        std::string s_msg;
    };

    struct xio_ctl_s
    {
        xio_msg_s xmsg;
        std::vector<std::string> *vec;
        uint64_t size;
    };

    void
    xio_send_open_request(const std::string& volname,
                          const void *opaque);

    void
    xio_send_close_request(const void *opaque);

    void
    xio_send_read_request(void *buf,
                          const uint64_t size_in_bytes,
                          const uint64_t offset_in_bytes,
                          const void *opaque);

    void
    xio_send_write_request(const void *buf,
                           const uint64_t size_in_bytes,
                           const uint64_t offset_in_bytes,
                           const void *opaque);

    void
    xio_send_flush_request(const void *opaque);

    int
    on_session_event(xio_session *session,
                     xio_session_event_data *event_data);

    int
    on_response(xio_session *session,
                xio_msg* reply,
                int last_in_rxq);

    int
    assign_data_in_buf(xio_msg *msg);

    int
    on_msg_error(xio_session *session,
                 xio_status error,
                 xio_msg_direction direction,
                 xio_msg *msg);

    void
    evfd_stop_loop(int fd, int events, void *data);

    bool
    is_queue_empty();

    xio_msg_s*
    pop_request();

    void
    push_request(xio_msg_s *req);

    void
    xstop_loop();

    static void
    xio_create_volume(const std::string& uri,
                      const std::string& volume_name,
                      size_t size,
                      void *opaque);

    static void
    xio_remove_volume(const std::string& uri,
                      const std::string& volume_name,
                      void* opaque);

    static void
    xio_stat_volume(const std::string& uri,
                    const std::string& volume_name,
                    void *opaque);

    static void
    xio_list_volumes(const std::string& uri,
                     std::vector<std::string>& volumes);

    static void
    xio_list_snapshots(const std::string& uri,
                       const std::string& volume_name,
                       std::vector<std::string>& snapshots,
                       uint64_t *size,
                       void *opaque);

    static void
    xio_create_snapshot(const std::string& uri,
                        const std::string& volume_name,
                        const std::string& snap_name,
                        int64_t timeout,
                        void *opaque);

    static void
    xio_delete_snapshot(const std::string& uri,
                        const std::string& volume_name,
                        const std::string& snap_name,
                        void *opaque);

    static void
    xio_rollback_snapshot(const std::string& uri,
                          const std::string& volume_name,
                          const std::string& snap_name,
                          void *opaque);

    static void
    xio_is_snapshot_synced(const std::string& uri,
                           const std::string& volume_name,
                           const std::string& snap_name,
                           void *opaque);
private:
    xio_context *ctx;
    xio_session *session;
    xio_connection *conn;
    xio_session_params params;
    xio_connection_params cparams;

    std::string uri_;
    bool stopping;
    std::thread xio_thread_;

    mutable fungi::SpinLock inflight_lock;
    std::queue<xio_msg_s*> inflight_reqs;

    xio_session_ops ses_ops;
    bool disconnected;

    int evfd;

    void
    xio_run_loop_worker(void *arg);

    static xio_connection*
    create_connection_control(xio_context *ctx,
                              const std::string& uri);

    static int
    on_msg_control(xio_session *session,
                   xio_msg *reply,
                   int last_in_rqx,
                   void *cb_user_context);

    static int
    on_msg_error_control(xio_session *session,
                         xio_status error,
                         xio_msg_direction direction,
                         xio_msg *msg,
                         void *cb_user_context);

    static int
    on_session_event_control(xio_session *session,
                             xio_session_event_data *event_data,
                             void *cb_user_context);

    static int
    assign_data_in_buf_control(xio_msg *msg,
                               void *cb_user_context);

    static void
    xio_submit_request(const std::string& uri,
                       xio_ctl_s *xctl,
                       void *opaque);

    static void
    xio_msg_prepare(xio_msg_s *xmsg);

    static void
    handle_list_volumes(xio_ctl_s *xctl,
                        xio_iovec_ex *sglist,
                        int vec_size);

    static void
    handle_list_snapshots(xio_ctl_s *xctl,
                          xio_iovec_ex *sglist,
                          int vec_size,
                          int size);
};

typedef std::shared_ptr<NetworkXioClient> NetworkXioClientPtr;

} //namespace volumedriverfs

#endif //__NETWORK_XIO_CLIENT_H_
