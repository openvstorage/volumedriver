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

#include "Mutex.h"
#include <youtils/IOException.h>

#include <cstring>
#include <cassert>
#include <errno.h>
namespace fungi {

Mutex::Mutex(const std::string & name,
             MutexType type)
    : name_(name)
{
    // Immanuel : Add error handling and name setting of mutex
    pthread_mutexattr_t mta;
    if(pthread_mutexattr_init(&mta))
    {
        LOG_ERROR("Could not init Mutex attributes");
    }
    switch(type)
    {
    case RecursiveMutex:
        if(pthread_mutexattr_settype(&mta,PTHREAD_MUTEX_RECURSIVE))
        {
            LOG_ERROR("Could not set the attribute type to recursive");
        }
        break;
    case Mutex::ErrorCheckingMutex:
#ifndef __sun
        if(pthread_mutexattr_settype(&mta,PTHREAD_MUTEX_ERRORCHECK_NP))
        {
            LOG_ERROR("Could not set the attribute type to error check");
        }
        break;
#else // fallback on normal mutex
        LOG_WARN("Cannot set chosen mutex attribute on this platform, might constitute a bug");
#endif
    case NormalMutex:
        // from the manpage:
        // The default mutex kind is ``fast'', that is, PTHREAD_MUTEX_FAST_NP.
        // No need to set anything.
        break;
    }

    if(pthread_mutex_init(&mutex_, &mta))
    {
        LOG_ERROR("Could not init mutex with attribute set");
    }
    if(pthread_mutexattr_destroy(&mta))
    {
        LOG_ERROR("Could not destroy mutex attributes");
    }
}

Mutex::~Mutex() {
	int res = pthread_mutex_destroy(&mutex_);
	if (res != 0) {
		// can't throw safely in destructor
#pragma warning ( push )
#pragma warning ( disable: 4996 )
		LOG_WARN("Mutex::~Mutex pthread_mutex_destroy " << strerror(res));
#pragma warning ( pop )
		assert(false); // TODO remove
	}
}

void Mutex::lock()
{
    int res = pthread_mutex_lock(&mutex_);
    // Yeah forse
    if (res != 0) {
        throw IOException("Mutex::lock", name_.c_str(), res);
    }
}

bool
Mutex::try_lock()
{
    int res = pthread_mutex_trylock(&mutex_);
    switch(res)
    {
    case  EBUSY:
        return false;

    case EINVAL:
        LOG_ERROR("Invalid mutex" << name_);
        throw IOException("Mutex::tryLock","Invalid",res);
    case 0:
        return true;
    default:
        throw IOException("Mutex::tryLock","Invalid",res);
    }
}


void Mutex::unlock() {
	int res = pthread_mutex_unlock(&mutex_);
	if (res != 0) {
		throw IOException("Mutex::unlock", "pthread_mutex_unlock", res);
	}
}

void
Mutex::assertLocked() const
{
    assert(pthread_mutex_trylock(&mutex_) == EBUSY);
}

pthread_mutex_t *Mutex::handle() {
	return &mutex_;
}

const std::string &Mutex::getName() const {
	return name_;
}

ScopedLock::ScopedLock(Mutex &m,
                       bool trylock)
    : m_(m)
{
    if(not trylock)
    {
        m_.lock();
    }
    else if (m_.try_lock())
    {

    }
    else
    {
        LOG_INFO("Couldn't get the lock " << m_.getName());
        throw fungi::IOException("Couldn't get the lock",
                                 m_.getName().c_str(),
                                 EBUSY);
    }
}

ScopedLock::~ScopedLock()
{
	m_.unlock();
}

}

// Local Variables: **
// mode: c++ **
// End: **
