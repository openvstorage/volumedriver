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
#include "Md5.h"
#include "WithGlobalLock.h"
#include "SourceOfUncertainty.h"
#include "SpinLock.h"
#include "StreamUtils.h"

#endif // YOUTILS_HEADERS
