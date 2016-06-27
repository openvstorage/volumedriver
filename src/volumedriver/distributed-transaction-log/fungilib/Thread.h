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

#ifndef _THREAD_H
#define	_THREAD_H

#include <pthread.h>
#include <youtils/Logging.h>

// implementation heavily based on asio::posix_thread
// interface inspired by the java Runnable/Thread interface

// TODO: Runnable is given by reference to the constructor but
// actually the thread takes full ownership. This is ugly and
// needs fixing.


namespace fungi {


    class Runnable;
    class Bubba;

    class Thread {
    	friend class Bubba; // avoid compiler warning
    public:
        DECLARE_LOGGER("Thread");

    	// remark: if a thread is not detached it HAS to be joined
    	// or you leak thread meta data, causing an out of memory
    	// error eventually
        // WARNING: If a thread is detached, the provided runnable object
        //          1- must be allocated from the heap
        //          2- is completely managed by the thread; the caller must
        //             never try to deallocate its memory.
        Thread(Runnable &runnable, bool detached = false);

        /** This method returns the name of the runnable object if still
         * referenced, otherwise it returns an empty string.
         * @return name of the runnable object if exists, otherwise an empty
         * string
         */
        const char* getRunObjName(void) const;
        void start();
        void join();
        void kill(int signal);

        /**
         * this method needs to be used to delete the thread
         * if it was a non-detached thread and it is not yet
         * joined this is considered a failure and will assert.
         * If asserts are disabled it will try to join() and
         * this can throw an exception
         * and exceptions are not allowed in destructors, hence
         * a separate call.
         * @exception IOException on join() failure
         */
        void destroy();
        void detach();
        bool isDetached()
        {
            return detached_;
        }

        // Immanuels private hacks!
        void
        StartNonDetachedWithCleanup();

        void
        StartWithoutCleanup();


    private:
    	~Thread();

        Thread();
        Thread(const Thread &);
        Thread & operator=(const Thread &);

        Runnable* runnable_;
        pthread_t thread_;
        pthread_attr_t attr_;
        bool joined_;
        bool detached_;
    };

}

#endif	/* _THREAD_H */

// Local Variables: **
// mode: c++ **
// End: **
