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

#ifndef COMBINEDTLOGBACKENDREADER_H_
#define COMBINEDTLOGBACKENDREADER_H_

#include "TLogReader.h"
#include "TLogReaderInterface.h"

#include <vector>
#include <string>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/utility.hpp>

#include <youtils/Generator.h>
#include <youtils/Logging.h>

namespace volumedriver
{

typedef boost::shared_ptr<TLogReaderInterface> TLogGenItem;
typedef youtils::Generator<TLogGenItem> TLogGen;

template<typename ReaderType,
         typename Items>
class TLogReaderGen
    : public TLogGen
{
public:
    TLogReaderGen(const boost::filesystem::path& tlog_path,
                  const Items& items,
                  BackendInterfacePtr bi)
        : tlog_path_(tlog_path)
        , next_index_(0)
        , bi_(std::move(bi))
        , items_(items.begin(),
                 items.end())
    {
        update();
    }

    void
    next()
    {
        update();
    }

    TLogGenItem&
    current()
    {
        return current_;
    }

    bool
    finished()
    {
        return not current_.get();
    }

private:
    void
    update()
    {
        if (next_index_ < items_.size())
        {
            const auto name(boost::lexical_cast<std::string>(items_[next_index_++]));
            if(bi_)
            {
                current_.reset(new ReaderType(tlog_path_,
                                              name,
                                              bi_->clone()));
            }
            else
            {
                current_.reset(new ReaderType(tlog_path_ / name));
            }
        }
        else
        {
           current_.reset();
        }
    }

    TLogGenItem current_;
    const boost::filesystem::path tlog_path_;
    uint64_t next_index_;
    BackendInterfacePtr bi_;
    const std::vector<typename Items::value_type> items_;
};

class CombinedTLogReader
    : public TLogReaderInterface
{
public:
    CombinedTLogReader(std::unique_ptr<TLogGen> gen)
        : generator(std::move(gen))
    {
        VERIFY(generator.get());
    }

    ~CombinedTLogReader() {}

    const Entry*
    nextAny()
    {
        if(not generator->finished())
        {
            const Entry* e = generator->current()->nextAny();

            if(e)
            {
                return e;
            }
            else
            {
                generator->next();
                return CombinedTLogReader::nextAny();
            }
        }
        else
        {
            return 0;
        }
    }

private:
    DECLARE_LOGGER("CombinedTLogReader");

    std::unique_ptr<TLogGen> generator;
};

}

#endif /* COMBINEDTLOGBACKENDREADER_H_ */

// Local Variables: **
// mode: c++ **
// End: **
