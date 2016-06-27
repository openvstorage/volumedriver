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

#ifndef _RUNNABLE_H
#define	_RUNNABLE_H
#include <youtils/Logging.h>


class ManagedRunnable;
extern "C" void *thread_start_util_(void *a);
extern "C" void *
second_thread_start_util(void* a);

namespace fungi {

    class Runnable {
    public:
        DECLARE_LOGGER("Runnable")
       ;
        Runnable() :
            fail_(false) {
        }
        virtual void run() = 0;
        virtual const char *getName() const = 0;
        bool failed() const;

        const std::string &what() const;

        virtual ~Runnable() {}
    protected:
        /**
         * @brief Sets the error string of this runnable thread
         *
         * @param[in] errStr error string to attach with this runnable
         */
        virtual void setErrStr_(const std::string& errStr);
        /**
         * @brief Sets the failure flag of this runnable thread
         */
        virtual void setFailure_(void);
    private:
        friend class ::ManagedRunnable;
        friend void* ::thread_start_util_(void* a);
        friend void* ::second_thread_start_util(void* a);

        bool fail_;
        std::string what_;
    };

}

#endif	/* _RUNNABLE_H */

// Local Variables: **
// mode: c++ **
// End: **
