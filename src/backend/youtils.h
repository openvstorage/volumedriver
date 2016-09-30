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

#ifndef YOUTILS_HPP_
#define YOUTILS_HPP_
#include "../youtils/thirdparty.h"
#include "../youtils/AlternativeOptions.h"
#include "../youtils/defines.h"
#include "../youtils/Readable.h"
#include "../youtils/UpdateReport.h"
#include "../youtils/Assert.h"
#include "../youtils/DimensionedValue.h"
#include "../youtils/Runnable.h"
#include "../youtils/UUID.h"
#include "../youtils/File.h"
#include "../youtils/Logger.h"
#include "../youtils/RWLock.h"
#include "../youtils/VolumeDriverComponent.h"
#include "../youtils/FileUtils.h"
#include "../youtils/Logging.h"
#include "../youtils/Serialization.h"
#include "../youtils/wall_timer.h"
#include "../youtils/BuildInfo.h"
#include "../youtils/Generator.h"
#include "../youtils/LowLevelFile.h"
#include "../youtils/Socket.h"
#include "../youtils/Md5.h"
#include "../youtils/ByteArray.h"
#include "../youtils/GlobalLockService.h"
#include "../youtils/Mutex.h"
#include "../youtils/SocketServer.h"
#include "../youtils/WithGlobalLock.h"
#include "../youtils/CheckSum.h"
#include "../youtils/IncrementalChecksum.h"
#include "../youtils/SpinLock.h"
#include "../youtils/WrapByteArray.h"
#include "../youtils/InitializedParam.h"
#include "../youtils/Notifier.h"
#include "../youtils/Streamable.h"
#include "../youtils/Writable.h"
#include "../youtils/CondVar.h"
#include "../youtils/IOBaseStream.h"
#include "<procon/ProCon.h>
#include "../youtils/StrongTypedString.h"
#include "../youtils/ConfigurationReport.h"
#include "../youtils/IOException.h"
#include "../youtils/SysUtils.h"
#include "../youtils/Conversions.h"
#include "../youtils/IPv4Socket.h"
#include "../youtils/ProtocolFactory.h"
#include "../youtils/Thread.h"
#include "../youtils/cpu_timer.h"
#include "../youtils/IPv6Socket.h"
#include "../youtils/Protocol.h"
#include "../youtils/System.h"
#endif // YOUTILS_HPP_

// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
