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

#ifndef YT_SHA1_H_
#define YT_SHA1_H_

#include "MessageDigest.h"

#include <openssl/sha.h>

namespace youtils
{

struct Sha1Traits
{
    static constexpr size_t digest_size = SHA_DIGEST_LENGTH;

    using Digest = std::array<uint8_t, digest_size>;
    using Context = SHA_CTX;

    static void
    digest(const uint8_t* buf,
           size_t size,
           Digest& digest)
    {
        SHA(buf, size, digest.data());
    }

    static int
    digest_init(Context& ctx)
    {
        return SHA1_Init(&ctx);
    }

    static int
    digest_update(Context& ctx,
                  const void* data,
                  const size_t size)
    {
        return SHA1_Update(&ctx,
                          data,
                          size);
    }

    static int
    digest_final(Context& ctx,
                 Digest& digest)
    {
        return SHA1_Final(digest.data(),
                         &ctx);
    }
};

using Sha1 = MessageDigest<Sha1Traits>;

}

BOOST_CLASS_VERSION(youtils::Sha1, 0);

#endif // !YT_SHA1_H_
