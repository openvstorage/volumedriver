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
