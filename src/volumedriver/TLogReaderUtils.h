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

#ifndef TLOGREADERUTILS_H
#define TLOGREADERUTILS_H

#include "BackwardTLogReader.h"
#include "CombinedTLogReader.h"
#include "TLogReader.h"
#include "TLogReaderInterface.h"

#include <memory>

#include <backend/BackendInterface.h>

namespace volumedriver
{

// Why templates you ask: The OrderedTLogIds are sufficient for everything except scrubbing
// where we treat the relocation as TLog. You can't type that as OrderedTLogIds.
template <typename T>
std::shared_ptr<TLogReaderInterface>
makeCombinedTLogReader(const fs::path& TLogPath,
                       const T& items,
                       BackendInterfacePtr bi,
                       bool prefetch = true)
{
    std::unique_ptr<TLogGen> generator;
    std::unique_ptr<TLogGen>
        baseGenerator(new TLogReaderGen<TLogReader, T>(TLogPath,
                                                       items,
                                                       std::move(bi)));

    if(prefetch)
    {
        generator.reset(new youtils::ThreadedGenerator<TLogGenItem>(std::move(baseGenerator),
                                                                    uint32_t(10)));
    }
    else
    {
        generator = std::move(baseGenerator);
    }

    auto reader(std::make_shared<CombinedTLogReader>(std::move(generator)));
    return std::static_pointer_cast<TLogReaderInterface>(reader);
}

template <typename T>
std::shared_ptr<TLogReaderInterface>
makeCombinedBackwardTLogReader(const fs::path& TLogPath,
                               const T& items,
                               BackendInterfacePtr bi)
{
    std::vector<typename T::value_type> rev_items(items.rbegin(),
                                                  items.rend());
    std::unique_ptr<TLogGen>
        generator(new TLogReaderGen<BackwardTLogReader, T>(TLogPath,
                                                           rev_items,
                                                           std::move(bi)));

    auto reader(std::make_shared<CombinedTLogReader>(std::move(generator)));
    return std::static_pointer_cast<TLogReaderInterface>(reader);
}

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
