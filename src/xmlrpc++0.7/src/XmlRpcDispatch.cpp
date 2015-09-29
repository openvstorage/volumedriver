
#include "XmlRpcDispatch.h"
#include "XmlRpcSource.h"
#include "XmlRpcUtil.h"
#include <vector>
#include <math.h>
#include <sys/timeb.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <assert.h>

#include <boost/foreach.hpp>

#if defined(_WINDOWS)
# include <winsock2.h>

# define USE_FTIME
# if defined(_MSC_VER)
#  define timeb _timeb
#  define ftime _ftime
# endif
#else
# include <sys/time.h>
#endif  // _WINDOWS


using namespace XmlRpc;


XmlRpcDispatch::XmlRpcDispatch()
{
  _endTime = -1.0;
  _doClear = false;
  _inWork = false;
}


XmlRpcDispatch::~XmlRpcDispatch()
{
}

// Monitor this source for the specified events and call its event handler
// when the event occurs
void
XmlRpcDispatch::addSource(XmlRpcSource* source, unsigned mask)
{
  _sources.push_back(MonitoredSource(source, mask));
}

bool
XmlRpcDispatch::hasSource(const XmlRpcSource* const source) const
{
    BOOST_FOREACH(const MonitoredSource& m, _sources)
    {
        if (m.getSource() == source)
        {
            return true;
        }
    }

    return false;
}

// Stop monitoring this source. Does not close the source.
void
XmlRpcDispatch::removeSource(XmlRpcSource* source)
{
  for (SourceList::iterator it=_sources.begin(); it!=_sources.end(); ++it)
    if (it->getSource() == source)
    {
      _sources.erase(it);
      break;
    }
}

// Modify the types of events to watch for on this source
void
XmlRpcDispatch::setSourceEvents(XmlRpcSource* source, unsigned eventMask)
{
  for (SourceList::iterator it=_sources.begin(); it!=_sources.end(); ++it)
    if (it->getSource() == source)
    {
      it->getMask() = eventMask;
      break;
    }
}

// Watch current set of sources and process events
void
XmlRpcDispatch::work(double timeout_in_seconds)
{
  // Compute end time
    _endTime = (timeout_in_seconds < 0.0) ? -1.0 : (getTime() + timeout_in_seconds);
  _doClear = false;
  _inWork = true;

  // Only work while there is something to monitor
  while (_sources.size() > 0) {
      // // Construct the sets of descriptors we are interested in
      // fd_set inFd, outFd, excFd;
      //       FD_ZERO(&inFd);
      //       FD_ZERO(&outFd);
      //       FD_ZERO(&excFd);

      //    int maxFd = -1;     // Not used on windows
      //    SourceList::iterator it;
    int fds_size = _sources.size();
    std::vector<struct pollfd> fds(fds_size);

    {

        int __offset = 0;
        for (SourceList::iterator it=_sources.begin(); it!=_sources.end(); ++it)
        {
            fds[__offset].fd = it->getSource()->getfd();
            fds[__offset].events = 0;
            fds[__offset].revents = 0;
            if (it->getMask() & ReadableEvent) fds[__offset].events |= POLLIN;
            if (it->getMask() & WritableEvent) fds[__offset].events |= POLLOUT;
            if (it->getMask() & Exception)     fds[__offset].events |= POLLERR;
            //  2      if (it->getMask() && fd > maxFd)   maxFd = fd;
            ++__offset;
        }
    }

    // Check for events
    returnhereoninterruptedsyscall:
    int nEvents;
    if (timeout_in_seconds < 0.0)
        nEvents = poll(&fds[0], fds_size, -1);
    else
    {
        nEvents = poll(&fds[0],fds_size, timeout_in_seconds * 1000);
    }
    if(nEvents == -1 and errno == EINTR)
        goto returnhereoninterruptedsyscall;

    if (nEvents < 0)
    {
        LOG_ERROR("Error in XmlRpcDispatch::work: error in select " <<  strerror(errno));
       //        LOG_ERROR(strerror(errno));

//        XmlRpcUtil::error("Error in XmlRpcDispatch::work: error in select (%d).", nEvents);
        _inWork = false;
        return;
    }

    // Process events
    {

        size_t __offset = 0;

        for (SourceList::iterator it = _sources.begin(); it != _sources.end(); )
        {
            SourceList::iterator thisIt = it++;
            XmlRpcSource* src = thisIt->getSource();
            unsigned newMask = (unsigned) -1;

            // accepting a new connection (XmlRpcServer::handleEvent)
            // will add a new entry to _sources but we don't have an
            // entry in the fds around.
            if (__offset < fds.size())
            {
                assert(fds[__offset].fd == src->getfd());
                // if (fd <= maxFd)
                // {
                // If you select on multiple event types this could be ambiguous
                if (fds[__offset].revents bitand POLLIN)
                {
                    newMask &= src->handleEvent(ReadableEvent);
                }
                if (fds[__offset].revents bitand POLLOUT)
                {
                    newMask &= src->handleEvent(WritableEvent);
                }
                if (fds[__offset].revents bitand POLLERR)
                {
                    newMask &= src->handleEvent(Exception);
                }
            }

            if (newMask == 0)
            {
                _sources.erase(thisIt);  // Stop monitoring this one
                if (!src->getKeepOpen())
                {
                    src->close();
                }

            }
            else if (newMask != (unsigned) -1)
            {
                thisIt->getMask() = newMask;
            }
            __offset++;
        }
    }

    // Check whether to clear all sources
    if (_doClear)
    {
      SourceList closeList = _sources;
      _sources.clear();
      for (SourceList::iterator it=closeList.begin(); it!=closeList.end(); ++it) {
	XmlRpcSource *src = it->getSource();
        src->close();
      }

      _doClear = false;
    }

    // Check whether end time has passed
    if (0 <= _endTime && getTime() > _endTime)
      break;
  }

  _inWork = false;
}



// Exit from work routine. Presumably this will be called from
// one of the source event handlers.
void
XmlRpcDispatch::exit()
{
  _endTime = 0.0;   // Return from work asap
}

// Clear all sources from the monitored sources list
void
XmlRpcDispatch::clear()
{
  if (_inWork)
    _doClear = true;  // Finish reporting current events before clearing
  else
  {
    SourceList closeList = _sources;
    _sources.clear();
    for (SourceList::iterator it=closeList.begin(); it!=closeList.end(); ++it)
      it->getSource()->close();
  }
}


double
XmlRpcDispatch::getTime()
{
#ifdef USE_FTIME
  struct timeb	tbuff;

  ftime(&tbuff);
  return ((double) tbuff.time + ((double)tbuff.millitm / 1000.0) +
	  ((double) tbuff.timezone * 60));
#else
  struct timeval	tv;
  struct timezone	tz;

  gettimeofday(&tv, &tz);
  return (tv.tv_sec + tv.tv_usec / 1000000.0);
#endif /* USE_FTIME */
}


// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// End: **
