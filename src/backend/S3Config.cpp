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

#include "S3Config.h"

#include <iostream>

namespace backend
{

std::ostream&
S3Config::stream_out(std::ostream& os) const
{
#define V(x)                                    \
    ((x).value())

    return os <<
        "S3Config{host=" << V(s3_connection_host) <<
        ",port=" << V(s3_connection_port) <<
        ",username=" << V(s3_connection_username) <<
        ",use_ssl=" << V(s3_connection_use_ssl) <<
        ",ssl_verify_host=" << V(s3_connection_ssl_verify_host) <<
        ",ssl_cert_file=" << V(s3_connection_ssl_cert_file) <<
        ",flavour=" << V(s3_connection_flavour) <<
        "}";

#undef V
}

}
