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

#include "../TestBase.h"
#include <boost/thread.hpp>

/* A */
namespace youtilstest
{
class GoTest : public TestBase
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
