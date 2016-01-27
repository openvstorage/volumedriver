// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef YOUTILS_HEADERS
#define YOUTILS_HEADERS
#include "ArakoonInterface.h"
#include "ArakoonNodeConfig.h"
#include "ArakoonTestSetup.h"
#include "Assert.h"
#include "BooleanEnum.h"
#include "BuildInfo.h"
#include "BuildInfoString.h"
#include "Catchers.h"
#include "CheckSum.h"
#include "Chooser.h"
#include "ConfigurationReport.h"
#include "CorbaTestSetup.h"
#include "cpu_timer.h"
#include "DimensionedValue.h"
#include "FileRange.h"
#include "FileUtils.h"
#include "FileDescriptor.h"
#include "Generator.h"
#include "GlobalLockedCallable.h"
#include "GlobalLockService.h"
#include "IncrementalChecksum.h"
#include "InitializedParam.h"
#include "IOException.h"
#include "ListContainingMap.h"
#include "LocORemClient.h"
#include "LocORemConnection.h"
#include "LocORemServer.h"
#include "Logger.h"
#include "LoggerPrivate.h"
#include "Logging.h"
#include "LoggingMacros.h"
#include "LoggingToolCut.h"
#include "LRUCache.h"
#include "MainCorba.h"
#include "MainEvent.h"
#include "Main.h"
#include "MainHelper.h"
#include "Notifier.h"
#include "OptionValidators.h"
#include "OrbHelper.h"
#include "OrbParameters.h"
#include "OurStrongTypedef.h"
#include "PeriodicAction.h"
#include <procon/ProCon.h>
#include "ProtobufLogger.h"
#include <pstreams/pstream.h>
#include "PThreadMutexLockable.h"
#include "RWLock.h"
#include "ScopeExit.h"
#include "SerializableDynamicBitset.h"
#include "Serialization.h"
#include "StrongTypedPath.h"
#include "StrongTypedString.h"
#include "System.h"
#include "TestBase.h"
#include "TestMainHelper.h"
#include "thirdparty.h"
#include "ThreadPool.h"
#include "TimeDurationType.h"
#include "Time.h"
#include "Tracer.h"
#include "UpdateReport.h"
#include "UUID.h"
#include "VolumeDriverComponent.h"
#include "WaitForIt.h"
#include "wall_timer.h"
#include "Weed.h"
#include "WithGlobalLock.h"
#include "SourceOfUncertainty.h"
#include "SpinLock.h"
#include "StreamUtils.h"

#endif // YOUTILS_HEADERS
