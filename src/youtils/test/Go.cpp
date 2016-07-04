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

#include <gtest/gtest.h>
#include <boost/thread.hpp>

/* A */
namespace youtilstest
{
class GoTest : public testing::Test
{
protected:
    void
    doSomethingForAWhile()
    {
        sleep(1);
    }


};

/*
c := make(chan int)  // Allocate a channel.
// Start the sort in a goroutine; when it completes, signal on the channel.
go func() {
    list.Sort()
    c <- 1  // Send a signal; value does not matter.
}()
doSomethingForAWhile()
<-c   // Wait for sort to finish; discard sent value.
*/

TEST_F(GoTest, DISABLED_challenge_the_first)
{
    std::list<int> lst;
    auto f = [&lst]() -> void { lst.sort();};
    boost::thread t(f);
    doSomethingForAWhile();
    t.join();
}
}
