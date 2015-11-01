// Copyright 2015 iNuron NV
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

#ifndef TRANSIENTEXCEPTION_H_
#define TRANSIENTEXCEPTION_H_

#include <youtils/IOException.h>

namespace volumedriver {

class TransientException : public fungi::IOException {
public:
	explicit TransientException(const char *msg,
				    const char *name = 0,
				    int error_code = 0);
};

}

#endif // !TRANSIENTEXCEPTION_H_
