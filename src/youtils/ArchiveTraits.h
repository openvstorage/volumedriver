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

#ifndef YT_ARCHIVE_TRAITS_H_
#define YT_ARCHIVE_TRAITS_H_

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

namespace youtils
{

template<typename Archive>
struct IsForwardCompatibleArchive
{
    enum { value = false };
};

template<>
struct IsForwardCompatibleArchive<boost::archive::xml_iarchive>
{
    enum { value = true };
};

template<>
struct IsForwardCompatibleArchive<boost::archive::xml_oarchive>
{
    enum { value = true };
};

template<typename Archive>
struct AssociatedOArchive;

template<>
struct AssociatedOArchive<boost::archive::binary_iarchive>
{
    using type = boost::archive::binary_oarchive;
};

template<>
struct AssociatedOArchive<boost::archive::text_iarchive>
{
    using type = boost::archive::text_oarchive;
};

template<>
struct AssociatedOArchive<boost::archive::xml_iarchive>
{
    using type = boost::archive::xml_oarchive;
};

}

#endif // !YT_ARCHIVE_TRAITS_H_
