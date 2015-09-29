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

#include "Thread.h"
#include "Runnable.h"
#include "ByteArray.h"
#include <youtils/IOException.h>
#include <youtils/Logging.h>

#include <signal.h>
#include <cerrno>
#include <cassert>
/**
 * @brief Module name(for debugging purposes)
 */
#define MOD_NAME "Thread"

// prctl(PR_SET_NAME) allows to rename threads, but is linux specific. To
// make things (only slightly) worse, PR_SET_NAME was introduced with 2.6.9.
// Code making use of it will be enclosed in #ifdef PR_SET_NAME.
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif /* HAVE_SYS_PRCTL_H */

using fungi::Runnable;
using std::string;
// decorator pattern
/** This class holds adds optional self memory management to runnable objects.
 */
class ManagedRunnable: public Runnable {
public:
    /** This constructor stores the object to be threaded and its memory managed
     * strategy.
     * @param[in] obj object to be threaded
     * @param[in] managed flag indicating whether the threaded object is owned
     * by the wrapper thread function, defaulting to not managed
     */
    ManagedRunnable(Runnable& obj, const bool managed = false);
    ~ManagedRunnable();
    const char* getName(void) const;
    void run(void);

protected:
    virtual void setErrStr_(const string& errStr);
    virtual void setFailure_(void);
private:
    /** object to be threaded */
    Runnable* const obj_;
    /** flag to indicate whether the active objected is owned by the thread
     * wrapper */
    const bool managed_;
};

ManagedRunnable::ManagedRunnable(Runnable& obj, const bool managed) :
    obj_(&obj), managed_(managed) {
}

ManagedRunnable::~ManagedRunnable() {

    // TODO Auto-generated destructor stub
    /// Get rid of the wrapped object if it's managed.
    if (managed_)
        delete obj_;

}

const char * ManagedRunnable::getName(void) const {
    return obj_->getName();
}

void ManagedRunnable::run(void) {

    obj_ -> run();

}

void ManagedRunnable::setErrStr_(const string& errStr) {

    Runnable::setErrStr_(errStr);
    obj_->setErrStr_(errStr);

}

void ManagedRunnable::setFailure_(void) {

    Runnable::setFailure_();
    obj_->setFailure_();

}

void *thread_start_util_(void *a) {
    std::auto_ptr<fungi::Runnable> r(static_cast<fungi::Runnable *>(a));
#ifdef PR_SET_NAME
    prctl(PR_SET_NAME, r->getName(), 0, 0, 0);
#endif
    string errStr;
    bool failed = true;
	try {

	    r->run();
        failed = false;

    } catch (std::exception& err) {

        errStr = err.what();

    } catch (...) {

        errStr = "An unknown exception occurred.";

	}

    if (failed) {

        r->setErrStr_(errStr);
        //        LOG_ERROR(MOD_NAME, errStr);

    }

	return 0;
}

void*
second_thread_start_util(void* a)
{
    std::pair<fungi::Runnable*, fungi::Thread*>* arg = static_cast<std::pair<fungi::Runnable*, fungi::Thread*> *>(a);
    fungi::Thread* thread = arg->second;
    fungi::Runnable* runnable = arg->first;
    delete arg;

#ifdef PR_SET_NAME
    prctl(PR_SET_NAME, runnable->getName(), 0, 0, 0);
#endif
    string errStr;
    bool failed = true;
    try
    {

        runnable->run();
        failed = false;

    }
    catch (std::exception& err)
    {

        errStr = err.what();

    }
    catch (...)
    {
        errStr = "An unknown exception occurred.";
    }
    try
    {
        if(thread->isDetached())
        {
            thread->destroy();
        }

    }
    catch(...)
    {
        //        LOG_FERROR("immmanuels_thread_start_util","Problem destroying thread");
    }

    if (failed)
    {
        runnable->setErrStr_(errStr);
        //        LOG_FERROR(MOD_NAME, errStr);
    }
    return 0;
}

namespace fungi {

    Thread::Thread(Runnable &runnable, bool detached) :
        runnable_(&runnable),
#ifndef _WIN32
        thread_(0),
#endif
        joined_(false), detached_(detached) {
        LOG_DEBUG("Thread::Thread("<< &runnable <<":"<< runnable.getName() <<")");
        pthread_attr_init(&attr_);
        if (detached) {
            pthread_attr_setdetachstate(&attr_, PTHREAD_CREATE_DETACHED);
        }
    }

    const char*
        Thread::getRunObjName(void) const {
        static const char defaultName[] = "";

        return runnable_ == NULL ? defaultName : runnable_ -> getName();

    }

Thread::~Thread() {
}

void Thread::destroy() {
	LOG_DEBUG("Thread::destroy("<<runnable_<<":"<<getRunObjName()<<")");
	if (!detached_) {
		if (!joined_) {
			LOG_WARN("Trying to destroy a non joined thread; please join it first!");
		}
		assert(joined_);
		join();
	}
	pthread_attr_destroy(&attr_);
	delete this;
}

void Thread::detach() {
	LOG_DEBUG("Thread::detach("<<runnable_<<":"<<getRunObjName()<<")");
	int res = pthread_detach(thread_);
	if (res != 0) {
		throw IOException("Thread::detach", "pthread_detach", res);
	}
	detached_ = true;
}

void Thread::start() {
	// TODO: make sure Thread::start() never throws
    ManagedRunnable* wrapper = new ManagedRunnable(*runnable_, detached_);

    // how does the auto_ptr help here in any way ????
    std::auto_ptr<ManagedRunnable> autoWrapper(wrapper);

    wrapper = NULL;/// Drop the explicit reference to the runnable wrapper.

#ifdef LOCKING_DEBUG
    const std::string rname = runnable_->getName();
#endif
    /// Drop the pointer to the runnable object if it's going to be managed by
    /// its wrapper.
    if (detached_) {
        runnable_ = NULL;
    }

	int res = pthread_create(&thread_, &attr_, thread_start_util_,
            autoWrapper.release());

#ifdef LOCKING_DEBUG
	LOG_WARN("LOCKING_DEBUG new thread: [" << std::hex << thread_ << std::dec <<
                 "] with runnable: " << rname);
#endif
	if (res != 0) {
		throw IOException("Thread::start", "pthread_create", res);
	}
}

void
Thread::StartNonDetachedWithCleanup()
{
    if(! runnable_)
    {
        LOG_ERROR("Null runnable, Thread already started??");
        throw fungi::IOException("Thread already started");
    }


    ManagedRunnable* wrapper = new ManagedRunnable(*runnable_, true);
    runnable_ = 0;
    int res = pthread_create(&thread_,
                             &attr_,
                             thread_start_util_,
                             wrapper);

    if (res != 0) {
        throw IOException("Thread::start", "pthread_create", res);
    }

}



void
Thread::StartWithoutCleanup()
{

    if(! runnable_)
    {
        LOG_ERROR("Null runnable, Thread already started??");
        throw fungi::IOException("Thread already started");
    }

    std::pair<Runnable*, Thread*>* pa= new std::pair<Runnable*, Thread*>(runnable_, this);
    runnable_ = 0;
    int res = pthread_create(&thread_,
                             &attr_,
                             &::second_thread_start_util,
                             pa);

    if (res != 0) {
        throw IOException("Thread::start", "pthread_create", res);
    }


}

void Thread::join() {
	if (!joined_) {
		int res = pthread_join(thread_, 0);
		joined_ = true;
		if (res == EDEADLK) {
			// thread tries to join itself; this shouldn't happen
			// in production we want to ignore this, but for debug
			// this is an error!
			LOG_ERROR("Thread tries to join itself; EDEADLCK!");
			assert(false);
			return;
		}
		// in win32 it could fail because we can't do the self test; so ignore the failure...
		if (res != 0) {
			throw IOException("Thread::join", "pthread_join", res);
		}
	}
}

void Thread::kill(int signal) {
	int res = pthread_kill(thread_, signal);
	if (res == ESRCH) {
		LOG_DEBUG("Thread: Failure to kill, thread probably already stopped (ignored)");
	}
	if (res != 0 && res != ESRCH) {
		throw IOException("Thread::kill", "pthread_kill", res);
	}
}

}

// Local Variables: **
// End: **
