// Copyright 2015 iNuron NV
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

#include "BackwardTLogReader.h"
#include "TLogReader.h"
#include "TLogReaderInterface.h"

#include <vector>
#include <string>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/utility.hpp>

#include <youtils/Generator.h>
#include <youtils/Logging.h>

#include <backend/BackendInterface.h>

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
    next() override final
    {
        update();
    }

    TLogGenItem&
    current() override final
    {
        return current_;
    }

    bool
    finished() override final
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

    ~CombinedTLogReader() = default;

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

    enum class FetchStrategy
    {
        Prefetch,
        OnDemand,
        Concurrent,
    };

    // Why templates you ask: The OrderedTLogIds are sufficient for everything except scrubbing
    // where we treat the relocation as TLog. You can't type that as OrderedTLogIds.
    template <typename T>
    static std::shared_ptr<TLogReaderInterface>
    create(const fs::path& tlog_path,
           const T& items,
           BackendInterfacePtr bi,
           const FetchStrategy strategy = FetchStrategy::Concurrent)
    {
        std::unique_ptr<TLogGen> generator;

        std::unique_ptr<TLogGen>
            base_generator(new TLogReaderGen<TLogReader, T>(tlog_path,
                                                           items,
                                                           std::move(bi)));

        switch (strategy)
        {
        case FetchStrategy::Prefetch:
            generator.reset(new youtils::PrefetchGenerator<TLogGenItem>(std::move(base_generator)));
            break;
        case FetchStrategy::Concurrent:
            generator.reset(new youtils::ThreadedGenerator<TLogGenItem>(std::move(base_generator),
                                                                        uint32_t(10)));
            break;
        case FetchStrategy::OnDemand:
            generator = std::move(base_generator);
            break;
        }

        VERIFY(generator);

        auto reader(std::make_shared<CombinedTLogReader>(std::move(generator)));
        return std::static_pointer_cast<TLogReaderInterface>(reader);
    }

    template <typename T>
    static std::shared_ptr<TLogReaderInterface>
    create_backward_reader(const fs::path& tlog_path,
                           const T& items,
                           BackendInterfacePtr bi)
    {
        std::vector<typename T::value_type> rev_items(items.rbegin(),
                                                      items.rend());
        std::unique_ptr<TLogGen>
            generator(new TLogReaderGen<BackwardTLogReader, T>(tlog_path,
                                                               rev_items,
                                                               std::move(bi)));

        auto reader(std::make_shared<CombinedTLogReader>(std::move(generator)));
        return std::static_pointer_cast<TLogReaderInterface>(reader);
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
