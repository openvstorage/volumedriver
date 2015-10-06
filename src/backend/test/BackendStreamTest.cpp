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

#include <fstream>

#include <boost/iostreams/stream.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem/fstream.hpp>

#include "backend/BackendConfig.h"
#include "backend/BackendInterface.h"
#include "backend/BackendException.h"
#include "../ManagedBackendSink.h"
#include "../ManagedBackendSource.h"
#include <youtils/FileUtils.h>

#include "BackendTestBase.h"

namespace backend
{
using namespace youtils;

class BackendStreamTest
    : public BackendTestBase
{
public:
    BackendStreamTest()
        : BackendTestBase("BackendStreamTest")
        , name_("object")
    {}

    virtual void
    SetUp()
    {
        BackendTestBase::SetUp();
        nspace_ = make_random_namespace();
    }

    virtual void
    TearDown()
    {
        nspace_.reset();
        BackendTestBase::TearDown();
    }


protected:
    std::unique_ptr<WithRandomNamespace> nspace_;
    const std::string name_;

    template<typename StreamGenerator>
    void
    istreamTooBigAReadTest(StreamGenerator& stream_gen)
    {
        if(not BackendTestSetup::streamingSupport())
        {
            SUCCEED () << "No streaming support on this backend, skipping test";
            return;
        }

        const std::string pattern("I started something I couldn't finish");
        const off_t wsize = 4096;

        {
            std::unique_ptr<std::ostream> os(stream_gen.getOutputStream());
            for (off_t i = 0; i < wsize; i += pattern.size())
            {
                size_t s = std::min(pattern.size(), size_t(wsize - i));
                os->write(pattern.c_str(), s);
            }

            os->flush();
            EXPECT_TRUE(os->good());
        }

        std::vector<char> readbuf(2 * wsize);
        std::unique_ptr<std::istream> is(stream_gen.getInputStream());

        is->read(&readbuf[0], readbuf.size());
        EXPECT_FALSE(is->good());
        EXPECT_TRUE(is->eof());

        is->clear();
        is->seekg(0, std::ios_base::end);
        EXPECT_TRUE(is->good());
        EXPECT_TRUE(wsize == is->tellg());
    }

    template<typename StreamGenerator>
    void
    istreamOpsTest(StreamGenerator& stream_gen)
    {
        const std::string pattern("Panic");
        const size_t count = 16384;
        const ssize_t size = count * (pattern.size() + 1);

        {
            std::unique_ptr<std::ostream> os = stream_gen.getOutputStream();
            for (size_t i = 0; i < count; ++i)
            {
                *os << pattern << std::endl;
            }

            EXPECT_TRUE(os->good());
        }

        std::unique_ptr<std::istream> is = stream_gen.getInputStream();

        is->seekg(0, std::ios_base::beg);
        EXPECT_TRUE(0 == is->tellg());

        is->seekg(0, std::ios_base::cur);
        EXPECT_TRUE(0 == is->tellg());

        is->seekg(0, std::ios_base::end);
        EXPECT_TRUE(size == is->tellg());
        EXPECT_TRUE(is->good());

        off_t off = 1;
        is->seekg(off, std::ios_base::end);
        EXPECT_TRUE(size + off == is->tellg());
        EXPECT_TRUE(is->good());

        std::vector<char> buf(pattern.size());
        is->read(&buf[0], buf.size());
        EXPECT_FALSE(is->good());
        EXPECT_TRUE(is->fail());

        is->clear();
        is->seekg(0, std::ios_base::beg);
        EXPECT_TRUE(0 == is->tellg());
        EXPECT_TRUE(is->good());

        for (size_t i = 0; i < count; ++i)
        {
            std::string s;
            *is >> s;
            EXPECT_TRUE(pattern == s);
        }

        {
            std::string s;
            *is >> s;
            EXPECT_TRUE("" == s);
        }

        EXPECT_TRUE(is->eof());
    }

    void
    testOStream(bool flush)
    {
        if(not BackendTestSetup::streamingSupport())
        {
            SUCCEED () << "No streaming support on this backend, skipping test";
            return;
        }

        const std::string objname("test");
        size_t size = 16 << 20;

        EXPECT_FALSE(bi_(nspace_->ns())->objectExists(objname));
        const std::string pattern("What difference does it make?");

        {
            std::unique_ptr<std::ostream> os(cm_->getOutputStream(nspace_->ns(),
                                                                  objname));

            for (size_t i = 0; i < size; i += std::min(size - i, pattern.size()))
            {
                size_t s = std::min(size - i, pattern.size());
                *os << pattern.substr(0, s);
                EXPECT_TRUE(os->good());
                // os->flush();
                // EXPECT_TRUE(os->good());
            }
            if (flush)
            {
                os->flush();
            }
            EXPECT_TRUE(os->good());
        }

        EXPECT_TRUE(bi_(nspace_->ns())->objectExists(objname));

        const fs::path dst(path_ / std::string(objname + ".dst"));
        EXPECT_TRUE(retrieveAndVerify(dst,
                                      size,
                                      pattern,
                                      0,
                                      cm_,
                                      objname,
                                      nspace_->ns()));

    }

    class FStreamGenerator
    {
    public:
        explicit FStreamGenerator(const fs::path& path)
            : path_(path)
        {}

        FStreamGenerator(const FStreamGenerator&) = delete;

        FStreamGenerator&
        operator=(const FStreamGenerator&) = delete;

        std::unique_ptr<std::ostream>
        getOutputStream()
        {
            // figure out why boost::ofstream does not work here
            return std::unique_ptr<std::ostream>(new std::ofstream(path_.string().c_str()));
        }

        std::unique_ptr<std::istream>
        getInputStream()
        {
            // figure out why boost::ifstream does not work here
            return std::unique_ptr<std::istream>(new std::ifstream(path_.string().c_str()));
        }

    private:
        const fs::path& path_;
    };

    class BackendStreamGenerator
    {
    public:
        BackendStreamGenerator(BackendConnectionManagerPtr conn_man,
                               const Namespace& nspace,
                               const std::string& name)
            : cm_(conn_man)
            , nspace_(nspace)
            , name_(name)
        {}

        BackendStreamGenerator(const BackendStreamGenerator&) = delete;

        BackendStreamGenerator&
        operator=(const BackendStreamGenerator&) = delete;

        std::unique_ptr<std::ostream>
        getOutputStream()
        {
            return cm_->getOutputStream(nspace_, name_);
        }

        std::unique_ptr<std::istream>
        getInputStream()
        {
            return cm_->getInputStream(nspace_, name_);
        }

    private:
        BackendConnectionManagerPtr cm_;
        const Namespace nspace_;
        const std::string name_;
    };
};

TEST_F(BackendStreamTest, rawSink)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    const std::string objname("test");
    const size_t size = 1 << 19;

    EXPECT_FALSE(bi_(nspace_->ns())->objectExists(objname));
    const std::string pattern("These things take time");

    {
        ManagedBackendSink sink(cm_,
                                nspace_->ns(),
                                objname);

        for (size_t i = 0; i < size; i += std::min(size - i, pattern.size()))
        {
            size_t s = std::min(size - i, pattern.size());
            sink.write(pattern.c_str(), s);
        }

        sink.close();
    }

    EXPECT_TRUE(bi_(nspace_->ns())->objectExists(objname));

    const fs::path dst(path_ / std::string(objname + ".dst"));
    EXPECT_TRUE(retrieveAndVerify(dst,
                                  size,
                                  pattern,
                                  0,
                                  cm_,
                                  objname,
                                  nspace_->ns()));
}

TEST_F(BackendStreamTest, rawSinkNoClose)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    const size_t size = 64 << 10;
    const std::string oname("test");

    EXPECT_FALSE(bi_(nspace_->ns())->objectExists(oname));
    const std::string pattern("I Started Something I Couldn't Finish");

    {
        ManagedBackendSink sink(cm_,
                                nspace_->ns(),
                                oname);
        for (size_t i = 0; i < size; i += std::min(size - i, pattern.size()))
        {
            const size_t s = std::min(size - i, pattern.size());
            sink.write(pattern.c_str(), s);
        }
    }

    EXPECT_FALSE(bi_(nspace_->ns())->objectExists(oname));
}

TEST_F(BackendStreamTest, ostream)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    testOStream(true);
}

TEST_F(BackendStreamTest, ostreamNoFlush)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    testOStream(false);
}

TEST_F(BackendStreamTest, rawSource)
{
    // terribly slow since we read pattern.size() bytes at a time, so don't
    // set the total size too big.
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    const size_t size = 1024 + 8;

    const std::string objname("test");
    const std::string pattern("Nowhere fast");
    const fs::path path(path_ / std::string(objname + ".src"));

    {
        EXPECT_THROW(ManagedBackendSource src(cm_,
                                              nspace_->ns(), objname),
                     BackendObjectDoesNotExistException);
    }

    const CheckSum chksum(createAndPut(path,
                                       size,
                                       pattern,
                                       cm_,
                                       objname,
                                       nspace_->ns(),
                                       OverwriteObject::F));

    ManagedBackendSource src(cm_,
                             nspace_->ns(),
                             objname);
    CheckSum chksum2;

    for (size_t i = 0; i < size; i += std::min(size - i, pattern.size()))
    {
        std::vector<char> buf(std::min(size - i, pattern.size()));
        EXPECT_EQ(static_cast<ssize_t>(buf.size()),
                  src.read(&buf[0], buf.size()));
        const std::string read(&buf[0], buf.size());
        EXPECT_EQ(read, pattern.substr(0, buf.size()));
        chksum2.update(&buf[0], buf.size());
    }

    EXPECT_EQ(chksum, chksum2);
}

TEST_F(BackendStreamTest, istream)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    const std::string objname("test");
    const size_t size = 65536 + 8;

    const std::string pattern("You've got everything now");
    const fs::path path(path_ / std::string(objname + ".src"));

    CheckSum chksum(createAndPut(path,
                                 size,
                                 pattern,
                                 cm_,
                                 objname,
                                 nspace_->ns(),
                                 OverwriteObject::F));

    std::unique_ptr<std::istream> is(cm_->getInputStream(nspace_->ns(),
                                                         objname));
    CheckSum chksum2;

    for (size_t i = 0; i < size; i += std::min(size - i, pattern.size()))
    {
        std::vector<char> buf(std::min(size - i, pattern.size()));
        is->read(&buf[0], buf.size());
        EXPECT_TRUE(is->good());

        const std::string read(&buf[0], buf.size());
        EXPECT_EQ(read, pattern.substr(0, buf.size())) << "iteration: " << i;
        chksum2.update(&buf[0], buf.size());
    }

    EXPECT_EQ(chksum, chksum2);
}

TEST_F(BackendStreamTest, rawSourceSeek)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    std::vector<std::string> patterns;

    patterns.push_back("The Smiths");
    patterns.push_back("Meat Is Murder");
    patterns.push_back("The Queen Is Dead");
    patterns.push_back("Strangeways Here We Come");

    const std::string objname("albums");
    EXPECT_FALSE(bi_(nspace_->ns())->objectExists(objname));
    off_t size = 0;

    {
        std::unique_ptr<std::ostream> os(cm_->getOutputStream(nspace_->ns(),
                                                              objname));
        BOOST_FOREACH(const std::string& s, patterns)
        {
            *os << s;
            size += s.size();
        }
    }

    EXPECT_TRUE(bi_(nspace_->ns())->objectExists(objname));

    ManagedBackendSource src(cm_,
                             nspace_->ns(),
                             objname);

    EXPECT_TRUE(0 == src.seek(0, std::ios_base::cur));
    EXPECT_TRUE(0 == src.seek(0, std::ios_base::beg));
    EXPECT_TRUE(size == src.seek(0, std::ios_base::end));

    for (std::vector<std::string>::reverse_iterator rit = patterns.rbegin();
         rit != patterns.rend();
         ++rit)
    {
        off_t off = -rit->size();

        src.seek(off, std::ios_base::cur);

        std::vector<char> buf(rit->size());
        src.read(&buf[0], buf.size());
        std::string read(&buf[0], buf.size());

        EXPECT_EQ(*rit, read);

        src.seek(off, std::ios_base::cur);
    }

    EXPECT_TRUE(0 ==src.seek(0, std::ios_base::cur));
}

TEST_F(BackendStreamTest, istreamSeek)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    std::vector<std::string> patterns;

    patterns.push_back("Hatful Of Hollow");
    patterns.push_back("Louder Than Bombs");
    patterns.push_back("The World Won't Listen");
    patterns.push_back("The Sound Of The Smiths");

    const std::string objname("compilations");
    EXPECT_FALSE(bi_(nspace_->ns())->objectExists(objname));

    {
        std::unique_ptr<std::ostream> os(cm_->getOutputStream(nspace_->ns(),
                                                              objname));
        BOOST_FOREACH(const std::string& s, patterns)
        {
            *os << s << std::endl;
        }
    }

    EXPECT_TRUE(bi_(nspace_->ns())->objectExists(objname));

    {
        std::unique_ptr<std::istream> is(cm_->getInputStream(nspace_->ns(), objname));

        is->seekg(0, std::ios_base::end);
        EXPECT_TRUE(bi_(nspace_->ns())->getSize(objname) == static_cast<unsigned>(is->tellg()));

        for (std::vector<std::string>::reverse_iterator rit = patterns.rbegin();
             rit != patterns.rend();
             ++rit)
        {
            off_t off = -(rit->size() + 1);

            is->seekg(off,
                      std::ios_base::cur);

            EXPECT_TRUE(is->good());

            std::vector<char> buf(rit->size() + 1);
            is->getline(&buf[0], buf.size());

            EXPECT_TRUE(is->good());

            std::string read(&buf[0], rit->size());
            EXPECT_EQ(*rit, read) << "tell: " ;

            // << is->tellg()

            is->seekg(off,
                      std::ios_base::cur);
        }

        EXPECT_TRUE(0 == is->tellg());
    }
}

TEST_F(BackendStreamTest, istreamOps)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    {
        // explore the behaviour of std::fstreams ...
        const fs::path p(path_ / "testfil");
        FStreamGenerator fsg(p);
        istreamOpsTest(fsg);
    }

    {
        // ... as the one of BackendStreams is supposed to match.
        const std::string objname("testobject");

        BackendStreamGenerator bsg(cm_,
                                   nspace_->ns(),
                                   objname);
        istreamOpsTest(bsg);
    }
}

TEST_F(BackendStreamTest, tooBigAReadTest)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }
    {
        // explore the behaviour of std::fstreams ...
        const fs::path p(path_ / "testfile");
        FStreamGenerator fsg(p);
        istreamTooBigAReadTest(fsg);
    }

    {
        // ... as the one of BackendStreams is supposed to match.
        const std::string objname("testobject");

        BackendStreamGenerator bsg(cm_,
                                   nspace_->ns(),
                                   objname);
        istreamTooBigAReadTest(bsg);
    }
}

// Raison d'etre is to observe the buffering behaviour by looking at the TRACE log
// messages (leading to the interesting observation that a seekg on an istream
// invalidates the read buffer even if the seek did not actually change the position).
//
// Otherwise completely boring and useless, hence disabled.
TEST_F(BackendStreamTest, DISABLED_readBuffering)
{
    if(not BackendTestSetup::streamingSupport())
    {
        SUCCEED () << "No streaming support on this backend, skipping test";
        return;
    }

    const size_t objsize = 5 << 20;
    const size_t bufsize = 1 << 19;
    const std::string objname("test");
    const std::string pattern("Is it really so strange?");
    {
        std::unique_ptr<std::ostream> os(cm_->getOutputStream(nspace_->ns(),
                                                              objname,
                                                              bufsize));
        for (size_t i = 0; i < objsize; i += pattern.size())
        {
            const size_t s = std::min(objsize - i, pattern.size());
            os->write(pattern.c_str(), s);
        }

        os->flush();
    }

    {
        std::unique_ptr<std::istream> is(cm_->getInputStream(nspace_->ns(),
                                                             objname,
                                                             bufsize));
        const size_t chunksize = 4096;
        std::vector<char> rbuf(chunksize);

        for (size_t i = 0; i < objsize; i += rbuf.size())
        {
            if (is->tellg() != static_cast<ssize_t>(i))
            {
                is->seekg(i);
            }

            const size_t s = std::min(objsize - i, rbuf.size());
            is->read(&rbuf[0], s);
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
