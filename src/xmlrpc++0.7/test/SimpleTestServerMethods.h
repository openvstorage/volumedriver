#ifndef SIMPLE_XMLRPC_TEST_SERVER_METHODS_H
#define SIMPLE_XMLRPC_TEST_SERVER_METHODS_H
#include "../src/XmlRpcServerMethod.h"
#include "../src/XmlRpcException.h"
#include <stdlib.h>
#include <limits>
#include "../src/XmlRpcValue.h"
namespace xmlrpctest
{
namespace x = XmlRpc;

template<typename T>
T
getUIntVal(x::XmlRpcValue& xval)
{
    std::string v(xval);
    char* tail;
    uint64_t val = strtoull(v.c_str(),
                            &tail,
                            10);
    if(val == 0 and tail != (v.c_str() + v.size()))
    {
        throw x::XmlRpcException("Could not convert " + v + " to unsigned int", 1);
    }
    if(val > std::numeric_limits<T>::max())
    {
        throw x::XmlRpcException("Argument too large " + v, 1);
    }
    return static_cast<T>(val);
}


class Counter
    : public x::XmlRpcServerMethod
{
public:
    explicit Counter()
        : x::XmlRpcServerMethod("counter")
        , count_(0)
    {}
    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;

private:
    virtual void
    execute(x::XmlRpcValue& /* params */,
            x::XmlRpcValue& /* result */)
    {
        ++count_;
    }

    virtual std::string
    help()
    {
        return name();
    }

private:
    uint32_t count_;
};

class HangAbout : public x::XmlRpcServerMethod
{
public:
    explicit HangAbout(uint32_t sleep)
        : x::XmlRpcServerMethod("hangabout")
        , sleep_(sleep)
    {}
    HangAbout(const HangAbout&) = delete;
    HangAbout& operator=(const HangAbout&) = delete;

private:
    virtual void
    execute(x::XmlRpcValue&  params,
            x::XmlRpcValue& /* result */)
    {
        if(not params[0].hasMember("sleep"))
        {
            sleep(sleep_);
        }
        else
        {
            sleep(getUIntVal<uint32_t>(params[0]));
        }
    }

    virtual std::string
    help()
    {
        return "do nothing for a while";
    }


private:
    uint32_t sleep_;
};


class Fawlty : public x::XmlRpcServerMethod
{
public:
    Fawlty()
        : x::XmlRpcServerMethod("fawlty")
    {}

    Fawlty(const Fawlty&) = delete;
    Fawlty& operator=(const Fawlty&) = delete;

private:
    virtual void
    execute(x::XmlRpcValue&  /*params*/,
            x::XmlRpcValue& /* result */)
    {
        throw x::XmlRpcException("Sounds like someone machine-gunning a seal.");
    }

    virtual std::string
    help()
    {
        return "throw an exception";
    }
};

}
#endif // SIMPLE_XMLRPC_TEST_SERVER_METHODS_H
// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
