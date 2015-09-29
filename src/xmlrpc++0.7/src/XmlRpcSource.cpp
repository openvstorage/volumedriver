
#include "XmlRpcSource.h"
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"

namespace XmlRpc {


  XmlRpcSource::XmlRpcSource(int fd /*= -1*/, bool deleteOnClose /*= false*/)
    : _fd(fd), _deleteOnClose(deleteOnClose), _keepOpen(false)
  {
  }

  XmlRpcSource::~XmlRpcSource()
  {
  }


  void
  XmlRpcSource::close()
  {
    if (_fd != -1) {
        LOG_DEBUG("Closing socket " << _fd);
        XmlRpcSocket::close(_fd);
        LOG_DEBUG("Done closing socket " << _fd);
        _fd = -1;
    }
    if (_deleteOnClose) {
        LOG_DEBUG("Deleting this");
        _deleteOnClose = false;
        delete this;
    }
  }

} // namespace XmlRpc


// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
