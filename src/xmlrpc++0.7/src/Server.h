#ifndef _CALLABLE_SERVER_H_
#define _CALLABLE_SERVER_H_

#include <stdint.h>
#include <memory>

#include <boost/thread.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <loki/Typelist.h>
#include <loki/TypelistMacros.h>

#include <youtils/Logging.h>
#include <youtils/Assert.h>

#include "ServerBase.h"
#include "XmlRpcServerMethod.h"
#include "MTServer.h"

namespace xmlrpc
{

template<class TList>
struct InstantiateXMLRPCS;

template<>
struct InstantiateXMLRPCS< ::Loki::NullType>
{
    static void inline
    doit(::xmlrpc::ServerBase* /*s*/,
         boost::ptr_vector< ::XmlRpc::XmlRpcServerMethod>& /*methods*/)
    {}
};

template<class T, class U>
struct InstantiateXMLRPCS< ::Loki::Typelist<T, U> >
{
    static void inline
    doit(::xmlrpc::ServerBase* s,
         boost::ptr_vector< ::XmlRpc::XmlRpcServerMethod>& methods)
    {
        methods.push_back(new T(s));
        InstantiateXMLRPCS<U>::doit(s,
                                    methods);
    }
};

enum class ServerType
{
    MT,
    ST
};

class Server
{
public:
    Server(const boost::optional<std::string>& addr,
           uint16_t port,
           uint16_t backlog = 32,
           ServerType servertype = ServerType::MT)
        : server_(servertype == ServerType::MT ?
                  static_cast<ServerBase*>(new MTServer()) :
                  static_cast<ServerBase*>(new XmlRpcServer()))
        , stop_(true)
        , thread_(nullptr)
        , servertype_(servertype)
    {
        server_->enableIntrospection(true);
        // addMethod(new SystemStop(*this));

        const bool started = server_->bindAndListen(addr,
                                                    port,
                                                    backlog);
        if(not started)
        {
            LOG_ERROR("Could not start xmlrpc server");
            throw fungi::IOException("Could not start xmlrpc server");
        }
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    ~Server()
    {
        stop_ = true;
        if(thread_)
        {
            thread_->join();
        }
    }

    void
    start()
    {
        ASSERT(not thread_);
        stop_ = false;
        thread_.reset(new boost::thread(boost::ref(*this)));
    }

    void
    stop()
    {
        ASSERT(thread_);
        stop_ = true;
    }

    void
    join()
    {
        ASSERT(thread_);
        thread_->join();
    }

    void
    operator()()
    {
        while(!stop_)
        {
            server_->work(1.0);
        }
        server_->exit();
        server_->shutdown();
    }

    ServerBase*
    getServer()
    {
        return server_.get();
    }

    template<typename T>
    void
    addMethods()
    {
        InstantiateXMLRPCS<T>::doit(getServer(), methods_);
    }

    void
    addMethod(std::unique_ptr<x::XmlRpcServerMethod> method)
    {
        methods_.push_back(method.get());
        server_->addMethod(method.release());
    }

    ServerType
    serverType() const
    {
        return servertype_;
    }

    DECLARE_LOGGER("XMLRPCSERVER");

private:
    std::unique_ptr<ServerBase> server_;
    bool stop_;
    std::unique_ptr<boost::thread> thread_;
    boost::ptr_vector<x::XmlRpcServerMethod> methods_;
    const ServerType servertype_;
};

// We could easily have the system stop passed parameters
template<typename T,
         void (T::*mem_fun)(void)>
class SystemStop : public x::XmlRpcServerMethod
{
public:
    SystemStop(Server& s,
               T& t)
        : XmlRpcServerMethod("system.stop")
        , server_(s)
        , t_(t)
        , stopping_(false)
    {}

    DECLARE_LOGGER("System Stop");

    SystemStop(const SystemStop&) = delete;

    SystemStop
    operator=(const SystemStop&) = delete;

    virtual void
    execute(x::XmlRpcValue& /*params*/,
            x::XmlRpcValue& /*result*/)
    {
        // Temporarily stop processing client requests and exit the work() method.

        {
            boost::unique_lock<boost::mutex> l(mtx_);
            if(stopping_)
            {
                throw x::XmlRpcException("Shutdown already in progress");
            }
            else
            {
                stopping_ = true;
            }
        }
        server_.getServer()->stopAcceptingConnections();

        LOG_INFO("Received xmlrpc stop call");

        try
        {
            (t_.*mem_fun)();

        }
        catch (std::exception& e)
        {
            LOG_ERROR("Cannot stop: " << e.what());
            // Make the previous thread detach, it will finish when the server finishes.
            server_.getServer()->startAcceptingConnections();
            stopping_ = false;

            throw x::XmlRpcException(std::string("Cannot Stop: ") + e.what());
        }
        catch (...)
        {
            LOG_ERROR("Cannot stop: unknown exception");
            server_.getServer()->startAcceptingConnections();
            stopping_ = false;
            throw x::XmlRpcException("Cannot Stop: Unknown Exception");
        }

        // LOG_INFO("Requesting XMLRPC server shutdown");
        server_.stop();
        // LOG_INFO("finished xmlrpc stop call");
    }

    virtual std::string
    help()
    {
        return std::string("Stop the server");
    }

private:
    Server& server_;
    T& t_;
    boost::mutex mtx_;
    bool stopping_;
};

}
#endif // _CALLABLE_SERVER_H_

// Local Variables: **
// mode: c++ **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// End: **
