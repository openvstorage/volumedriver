// Copyright (C) 2017 iNuron NV
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

#ifndef YT_MULTI_ARCHIVE_PERSISTOR_H_
#define YT_MULTI_ARCHIVE_PERSISTOR_H_

#include "ArchiveTraits.h"

#include <iosfwd>
#include <boost/serialization/nvp.hpp>

#include <loki/Typelist.h>
#include <loki/TypelistMacros.h>

namespace youtils
{

template<typename T>
struct MultiArchivePersistor;

template<>
struct MultiArchivePersistor<Loki::NullType>
{
    template<typename T>
    static void
    save(std::ostream&,
              const char*,
              const T&)
    {}

    template<typename T>
    static void
    load(std::istream&,
         const char*,
         T&,
         bool& end)
    {
        end = true;
    }
};

template<typename IArchive,
         typename Tail>
struct MultiArchivePersistor<Loki::Typelist<IArchive,
                                            Tail>>
{
    using OArchive = typename AssociatedOArchive<IArchive>::type;

    template<typename T>
    static void
    save(std::ostream& os,
         const char* nvp,
         const T& t)
    {
        {
            OArchive oa(os);
            oa << boost::serialization::make_nvp(nvp,
                                                 t);
        }

        MultiArchivePersistor<Tail>::save(os,
                                          nvp,
                                          t);
    }

    template<typename T>
    static void
    load(std::istream& is,
         const char* nvp,
         T& t)
    {
        bool end = false;
        load(is,
             nvp,
             t,
             end);
    }

    template<typename T>
    static void
    load(std::istream& is,
         const char* nvp,
         T& t,
         bool end)
    {
        T tmp;

        {
            IArchive ia(is);
            ia >> boost::serialization::make_nvp(nvp,
                                                 tmp);
        }

        try
        {
            // is.peek() != std::istream::traits_type::eof() would be nicer, but
            // tests suggest that deserializing a text_iarchive leaves a newline
            // on the stream? So let's treat any error as an absent archive.
            MultiArchivePersistor<Tail>::load(is,
                                              nvp,
                                              t,
                                              end);
            if (end)
            {
                t = std::move(tmp);
            }
        }
        catch (...)
        {
            t = std::move(tmp);
        }
    }
};

}

#endif // !YT_MULTI_ARCHIVE_PERSISTOR_H_
