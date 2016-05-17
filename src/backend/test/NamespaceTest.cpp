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

#include "../BackendTestSetup.h"
#include "../BackendException.h"
#include "../BackendConnectionInterface.h"
#include <youtils/TestBase.h>
#include "../Namespace.h"
#include <string.h>
namespace backendtest
{
class NamespaceTest
    : public youtilstest::TestBase
{};
using backend::Namespace;
namespace be = backend;


TEST_F(NamespaceTest, namespace_names)
{
    boost::optional<Namespace> ns;
    using namespace std::string_literals;

    ASSERT_THROW(Namespace ns(std::string(""s)),
                 be::Namespace::TooShortException);
    ASSERT_THROW(Namespace ns("aa"s),
                 be::Namespace::TooShortException);

    ASSERT_THROW(Namespace ns("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"s),
                 be::Namespace::TooLongException);

    ASSERT_THROW(Namespace ns(".myawsbucket"s),
                 be::Namespace::LabelTooShortException);

    ASSERT_THROW(Namespace ns("myawsbucket."s),
                 be::Namespace::LabelTooShortException);

    ASSERT_THROW(Namespace ns("my..examplebucket"s),
                 be::Namespace::LabelTooShortException);

    ASSERT_NO_THROW(Namespace ns("my.examplebucket"s));

    ASSERT_THROW(Namespace ns("my.exampleBucket"s),
                 be::Namespace::LabelNameException);
}


}
