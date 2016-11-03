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

#ifndef YT_WEED_H_
#define YT_WEED_H_

#include "MessageDigest.h"

#include <openssl/md5.h>

namespace youtils
{

struct Md5Traits
{
    static constexpr size_t digest_size = MD5_DIGEST_LENGTH;

    using Digest = std::array<uint8_t, digest_size>;
    using Context = MD5_CTX;

    static void
    digest(const uint8_t* buf,
           size_t size,
           Digest& digest)
    {
        MD5(buf, size, digest.data());
    }

    static int
    digest_init(Context& ctx)
    {
        return MD5_Init(&ctx);
    }

    static int
    digest_update(Context& ctx,
                  const void* data,
                  const size_t size)
    {
        return MD5_Update(&ctx,
                          data,
                          size);
    }

    static int
    digest_final(Context& ctx,
                 Digest& digest)
    {
        return MD5_Final(digest.data(),
                         &ctx);
    }
};


using Md5 = MessageDigest<Md5Traits>;
using Weed = Md5;

}

BOOST_CLASS_VERSION(youtils::Md5, 0);

#endif // YT_WEED_H_

// Local Variables: **
// mode: c++ **
// End: **
