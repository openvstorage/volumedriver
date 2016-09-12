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

#include "S3_Connection.h"
#include "BackendException.h"

#include <boost/chrono.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

// #define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
// #include <cryptopp/md5.h>
#include <webstor/wsconn.h>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/CheckSum.h>
#include <youtils/FileUtils.h>
#include <youtils/IOException.h>
#include <youtils/FileDescriptor.h>
#include <youtils/Weed.h>


namespace backend
{

namespace s3
{

namespace fs = boost::filesystem;
namespace ws = webstor;
namespace yt = youtils;

using namespace webstor;

#define NO_REQUEST_CONTEXT NULL
#define NO_GET_CONDITIONS NULL
#define NO_PUT_PROPERTIES NULL

Connection::Connection(const config_type& cfg)
    : Connection(cfg.s3_connection_flavour.value(),
                 cfg.s3_connection_host.value(),
                 cfg.s3_connection_port.value(),
                 cfg.s3_connection_username.value(),
                 cfg.s3_connection_password.value(),
                 cfg.s3_connection_verbose_logging.value(),
                 cfg.s3_connection_use_ssl.value() ? UseSSL::T : UseSSL::F,
                 cfg.s3_connection_ssl_verify_host.value() ?
                 SSLVerifyHost::T :
                 SSLVerifyHost::F,
                 cfg.s3_connection_ssl_cert_file.value())
{}

Connection::Connection(S3Flavour flavour,
                       const std::string host,
                       const uint16_t port,
                       const std::string& username,
                       const std::string& password,
                       bool verbose_logging,
                       const UseSSL use_ssl,
                       const SSLVerifyHost ssl_verify_host,
                       const fs::path& ssl_cert_file)
    : flavour_(flavour)
    , host_(host)
    , port_(port)
    , username_(username)
    , password_(password)
    , verbose_logging_(verbose_logging)
    , use_ssl_(use_ssl)
    , ssl_verify_host_(ssl_verify_host)
    , ssl_cert_file_(ssl_cert_file)
{
    setup_();
}

namespace
{

DECLARE_LOGGER("S3Connection");

int
trace_callback(void* /* handle */,
               ws::TraceInfo tinfo,
               unsigned char* data,
               size_t size,
               void* /* cookie */)
{
    static const char* desc[] = {
        "TEXT",
        "HDR_IN",
        "HDR_OUT",
        "DATA_IN",
        "DATA_OUT",
        "SSL_DATA_IN",
        "SSL_DATA_OUT"
    };

    switch (tinfo)
    {
    case WS_TRACE_INFO_TEXT:
    case WS_TRACE_INFO_HEADER_IN:
    case WS_TRACE_INFO_HEADER_OUT:
        LOG_TRACE(desc[tinfo] << ": " <<
                  std::string(reinterpret_cast<const char*>(data), size));
        break;
    case WS_TRACE_INFO_DATA_IN:
    case WS_TRACE_INFO_DATA_OUT:
    case WS_TRACE_INFO_SSL_DATA_IN:
    case WS_TRACE_INFO_SSL_DATA_OUT:
        LOG_TRACE(desc[tinfo] << ": (" << size << " bytes of data omitted)");
        break;
    default:
        LOG_TRACE("Invalid TraceInfo value " << tinfo);
    }

    return 0;
}

ws::WsStorType
translate_s3_type(S3Flavour f)
{
    switch (f)
    {
    case S3Flavour::S3:
        return ws::WsStorType::WST_S3;
    case S3Flavour::GCS:
        return ws::WsStorType::WST_GCS;
    case S3Flavour::WALRUS:
        return ws::WsStorType::WST_WALRUS;
    case S3Flavour::SWIFT:
        return ws::WsStorType::WST_SWIFT;
    }

    UNREACHABLE;
    return ws::WsStorType::WST_S3;
}

}

std::unique_ptr<ws::WsConnection>
Connection::new_connection_() const
{
    ws::WsConfig config;
    config.accKey = username_.c_str();
    config.secKey = password_.c_str();
    config.host = host_.c_str();
    config.storType = translate_s3_type(flavour_);

    config.isHttps = (use_ssl_ == UseSSL::T);
    config.sslCertFile = ssl_cert_file_.empty() ? nullptr : ssl_cert_file_.string().c_str();
    config.sslVerifyHost = (ssl_verify_host_ == SSLVerifyHost::T);

    const std::string port_str(boost::lexical_cast<std::string>(port_));
    config.port = port_str.c_str();

    config.proxy = nullptr;

    std::unique_ptr<ws::WsConnection> conn(new ws::WsConnection(config));
    conn->enableTracing(trace_callback);
    return conn;
}

void
Connection::setup_()
{
    ws_connection_ = new_connection_();
}

namespace
{

void
take_a_nap()
{
    boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
}

}

void
Connection::listNamespaces_(std::list<std::string>& /*objects*/)
{
    TODO("Y42: TO BE IMPLEMENTED");
    throw BackendNotImplementedException();
}

void
Connection::createNamespace_(const Namespace& nspace,
                             const NamespaceMustNotExist must_not_exist)
{
    if (must_not_exist == NamespaceMustNotExist::T and
        namespaceExists_(nspace))
    {
        LOG_ERROR("Namespace " << nspace << " already exists");
        throw BackendCouldNotCreateNamespaceException();
    }

    try
    {
        ws_connection_->createBucket(nspace.c_str(),
                                     false);
    }
    catch(WsException& e)
    {
        LOG_ERROR("Could not create namespace " << e.what());
        throw BackendCouldNotCreateNamespaceException();
    }

    unsigned retry = 0;

    while(not namespaceExists_(nspace))
    {
        LOG_INFO("Waiting for the namespace " << nspace <<
                 " to become visible, retry " << ++retry);
        take_a_nap();
    }
}

void
Connection::deleteNamespace_(const Namespace& ns)
{
    try
    {
        ws_connection_->delAll(ns.c_str(),
                               "");
        ws_connection_->delBucket(ns.c_str());
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Could not delete namespace " << ns << ": " << EWHAT);
            throw BackendCouldNotDeleteNamespaceException();
        });

    unsigned retry = 0;

    while(namespaceExists_(ns))
    {
        LOG_INFO("Waiting for the namespace " << ns << " to become invisible, retry " <<
                 ++retry);
        take_a_nap();
    }
}

void
Connection::clearNamespace_(const Namespace& ns)
{
    try
    {
        ws_connection_->delAll(ns.c_str(),
                               "");
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Could not clear namespace " << ns << ": " << EWHAT);
            throw BackendCouldNotClearNamespaceException();
        });

    unsigned retry = 0;

    while (getSize_(ns, "") > 0)
    {
        LOG_INFO("Waiting for the namespace " << ns << " to be cleared, retry " <<
                 ++retry);
        take_a_nap();
    }
}

bool
Connection::namespaceExists_(const Namespace& nspace)
{

    // char location_constraint_buf[64];
    std::vector< WsBucket > buckets;

    ws_connection_->listAllBuckets(&buckets);
    for(const auto& bucket : buckets)
    {
        if (bucket.name == nspace.str())
        {
            return true;
        }
    }
    return false;
}

namespace
{

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");

struct ObjectExistsCallback
    : public WsObjectEnum
{
    ObjectExistsCallback(const std::string& name)
        : name_ (name)
        , result_(false)
    {}

    virtual ~ObjectExistsCallback() = default;

    bool
    onObject(const WsObject& object) override final
    {
        if (object.key == name_)
        {
            result_ = true;
        }
        return true;
    }

    const std::string& name_;
    bool result_;
};

PRAGMA_IGNORE_WARNING_END;

}

bool
Connection::objectExists_(const Namespace& nspace,
                          const std::string& name)
{
    ObjectExistsCallback cb(name);

    try
    {
        ws_connection_->listAllObjects(nspace.c_str(),
                                       name.c_str(),
                                       "",
                                       &cb);
        return cb.result_;
    }
    catch(WsException& e)
    {
        LOG_ERROR("S3 exception " << e.what());
        throw BackendFatalException();
    }
}

namespace
{

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");

struct ObjectSizeCallback
    : public WsObjectEnum
{
    ObjectSizeCallback(const std::string& name)
        :name_(name)
        , size_(-1)
    {}

    virtual ~ObjectSizeCallback() = default;

    bool
    onObject(const WsObject& object) override final
    {
        if (object.key == name_)
        {
            size_ = object.size;
        }
        return true;

    }

    const std::string& name_;
    ssize_t size_;
};

PRAGMA_IGNORE_WARNING_END;

}

uint64_t
Connection::getSize_(const Namespace& nspace,
                     const std::string& name)
{
    ObjectSizeCallback cb(name);

    try
    {
        ws_connection_->listAllObjects(nspace.c_str(),
                                       name.c_str(),
                                       "",
                                       &cb);

        if(cb.size_ < 0)
        {
            throw BackendObjectDoesNotExistException();
        }
        else
        {
            return cb.size_;
        }
    }
    catch(WsException& e)
    {
        LOG_ERROR("S3 exception " << e.what());
        throw BackendFatalException();
    }
}

yt::CheckSum
Connection::getCheckSum_(const Namespace&,
                         const std::string& /*name*/)
{
    if(verbose_logging_)
    {
        LOG_FATAL("Getting a checksum is not implemented on an S3 backend");
    }

    throw BackendNotImplementedException();
}

void
Connection::remove_(const Namespace& nspace,
                    const std::string& name,
                    const ObjectMayNotExist may_not_exist,
                    const boost::shared_ptr<Condition>& cond)
{
    if (cond)
    {
        LOG_ERROR("conditional write support is not available yet for S3 backend");
        throw BackendNotImplementedException();
    }

    try
    {
        ws_connection_->del(nspace.c_str(),
                            name.c_str());
    }
    catch (WsStatusResourceNotFoundException&e)
    {
        if(T(may_not_exist))
        {
            return;
        }
        else
        {
            LOG_ERROR("Could not delete " << name
                      << " from " << nspace
                      << ": " << e.what());

            throw BackendObjectDoesNotExistException();
        }
    }
    catch(WsException& e)
    {
        LOG_ERROR("Could not delete " << name
                  << " from " << nspace
                  << ": " << e.what());

        throw BackendException(e.what());
    }
}

namespace
{

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");

struct ListObjectsCallback
    : public WsObjectEnum
{
    ListObjectsCallback(std::list<std::string>& out)
        : out_(out)
    {}

    virtual ~ListObjectsCallback() = default;

    bool
    onObject(const WsObject& object) override final
    {
        out_.push_back(object.key);
        return true;
    }
    std::list<std::string>& out_;
};

PRAGMA_IGNORE_WARNING_END;

}

void
Connection::listObjects_(const Namespace& nspace,
                         std::list<std::string>& out)
{
    ListObjectsCallback cb(out);

    try
    {
        ws_connection_->listAllObjects(nspace.c_str(),
                                       "",
                                       "",
                                       &cb);
    }
    catch(WsException& e)
    {
        LOG_ERROR("Could not list backend objects: " << e.what());
        throw BackendFatalException();
    }
}

namespace
{

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");

struct ObjectReadCallback
    : public WsGetResponseLoader
{
    ObjectReadCallback(const fs::path& destination)
        : sio_(destination,
               youtils::FDMode::Write,
               CreateIfNecessary::T)
    {}

    virtual ~ObjectReadCallback()
    {
        // sio_.close();
    }

    size_t
    onLoad(const void* chunkData,
           size_t chunkSize,
           size_t /*totalSizeHint*/) override final
    {
        // Y42 see how this works and
        sio_.write(chunkData,
                  chunkSize);
        return chunkSize;
    }

    youtils::FileDescriptor sio_;
};

PRAGMA_IGNORE_WARNING_END;

}

void
Connection::read_(const Namespace& nspace,
                  const fs::path& destination,
                  const std::string& name,
                  InsistOnLatestVersion insist_on_latest)
{
    try
    {
        WsGetResponse get_response;
        ObjectReadCallback cb(destination);

        ws_connection_->get(nspace.c_str(),
                            name.c_str(),
                            &cb,
                            &get_response,
                            (insist_on_latest == InsistOnLatestVersion::T));

        if(get_response.loadedContentLength == (size_t)-1)
        {
            // youtils::FileUtils::removeFileNoThrow(cb.sio_.path());
            throw BackendObjectDoesNotExistException();
        }
        // else
        // {
        //     fs::rename(cb.sio_.path(),
        //                destination);
        // }

        VERIFY(fs::exists(destination));
    }
    catch (WsStatusResourceNotFoundException&)
    {
        throw BackendObjectDoesNotExistException();
    }
    catch(WsException& e)
    {
        LOG_ERROR("GET failed " << e.what());
        throw BackendRestoreException();
    }
}

namespace
{

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");


// We walk over the file twice - first to get the MD5 (and while at it, also the CRC),
// and then (when called by webstor / curl) to perform the actual upload.
// The idea (hope) is that the second time the data already resides in the page cache.
// We might want to give the kernel a hint (cf. fadvise) or keep it in a buffer (at
// least if the size doesn't exceed a certain limit?).
struct ObjectWriteCallback
    : public WsPutRequestUploader
{
    ObjectWriteCallback(const fs::path& source,
                        const yt::CheckSum* cs)
        : sio_(source,
               youtils::FDMode::Read,
               CreateIfNecessary::F)
    {
        LOG_TRACE(source);

        VERIFY(fs::exists(source));

        std::vector<byte> rbuf(32 * 1024);

        size_t res = 0;
        yt::CheckSum crc;
        // CryptoPP::Weak::MD5 md5;
        MD5_CTX md5_context;
        MD5_Init(&md5_context);

        do
        {
            res = sio_.read(rbuf.data(), rbuf.size());
            if (cs)
            {
                crc.update(rbuf.data(), res);
            }

            MD5_Update(&md5_context,
                       rbuf.data(),
                       res);
        }
        while (res);

        MD5_Final(digest_.data(),
                  &md5_context);

        if (cs and *cs != crc)
        {
            LOG_ERROR(source << ": checksum mismatch - expected " << *cs << ", got " <<
                      crc);
            throw BackendInputException();
        }

        sio_.seek(0, yt::Whence::SeekSet);

    }

    virtual ~ObjectWriteCallback() = default;

    size_t
    onUpload(void* chunkBuf, size_t chunkSize) override final
    {
        LOG_TRACE(sio_.path() << ": reading " << chunkSize << " bytes");

        return sio_.read(chunkBuf,
                         chunkSize);
    }

    yt::Whence
    whence_from_ws_(ws::WsPutRequestUploader::SeekWhence whence)
    {
        switch (whence)
        {
        case ws::WsPutRequestUploader::SeekWhence::Set:
            return yt::Whence::SeekSet;
        case ws::WsPutRequestUploader::SeekWhence::Cur:
            return yt::Whence::SeekCur;
        case ws::WsPutRequestUploader::SeekWhence::End:
            return yt::Whence::SeekEnd;
        }

        UNREACHABLE;
    }

    ws::WsPutRequestUploader::SeekResult
    onSeek(off_t off, ws::WsPutRequestUploader::SeekWhence whence) override final
    {
        LOG_TRACE(sio_.path() << ": off " << off << ", whence " <<
                  static_cast<int>(whence));

        try
        {
            sio_.seek(off, whence_from_ws_(whence));
            return ws::WsPutRequestUploader::SeekResult::Ok;
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(sio_.path() << ": failed to seek to off " << off <<
                          " (whence: " << static_cast<int>(whence) << ")" << ": " << EWHAT);
                return ws::WsPutRequestUploader::SeekResult::Fail;
            });
    }

    DECLARE_LOGGER("ObjectWriteCallback");

    youtils::FileDescriptor sio_;
    ws::MD5Digest digest_;
};

PRAGMA_IGNORE_WARNING_END;

}

void
Connection::write_(const Namespace& nspace,
                   const fs::path& location,
                   const std::string& name,
                   const OverwriteObject overwrite_object,
                   const yt::CheckSum* cs,
                   const boost::shared_ptr<Condition>& cond)
{
    if (cond)
    {
        LOG_ERROR("conditional write support is not available yet for S3 backend");
        throw BackendNotImplementedException();
    }

    if(F(overwrite_object) and objectExists_(nspace,
                                             name))
    {
        LOG_ERROR("object " << name << " already exists on the backend " << nspace <<
                  " and you didn't ask overwrite ");
        throw BackendOverwriteNotAllowedException();
    }

    ObjectWriteCallback cb(location, cs);

    try
    {
        ws_connection_->put(nspace.c_str(),
                            name.c_str(),
                            &cb,
                            fs::file_size(location),
                            &cb.digest_);

        VERIFY(fs::exists(location));
    }
    catch(WsException& e)
    {
        LOG_ERROR("PUT failed " << e.what());
        throw BackendStoreException();
    }
}


std::unique_ptr<yt::UniqueObjectTag>
Connection::get_tag_(const Namespace&,
                     const std::string&)
{
    LOG_ERROR("yt::UniqueObjectTag support is not available for S3 backend");
    throw BackendNotImplementedException();
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::write_tag_(const Namespace&,
                       const fs::path&,
                       const std::string&,
                       const yt::UniqueObjectTag*,
                       const OverwriteObject)
{
    LOG_ERROR("yt::UniqueObjectTag support is not available for S3 backend");
    throw BackendNotImplementedException();
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::read_tag_(const Namespace&,
                      const fs::path&,
                      const std::string&)
{
    LOG_ERROR("yt::UniqueObjectTag support is not available yet for Alba backend");
    throw BackendNotImplementedException();
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
