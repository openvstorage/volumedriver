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
#include "NoGlobalLockingService.h"
#include "Notifier.h"
#include "OptionValidators.h"
#include "OrbHelper.h"
#include "OrbParameters.h"
#include "OurStrongTypedef.h"
#include "PeriodicAction.h"
#include "ProCon.h"
#include "ProtobufLogger.h"
#include "pstream.h"
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
