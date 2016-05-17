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

#ifndef VOLUMEDRIVER_HPP_
#define VOLUMEDRIVER_HPP_
#include "../backend/youtils.h"
#include "../backend/BackendConfig.h"
#include "../backend/BackendParameters.h"
#include "../backend/HttpHeader.h"
#include "../backend/ObjectInfo.h"
#include "../backend/BackendConfigOptions.h"
#include "../backend/BackendPolicyConfig.h"
#include "../backend/Local_Connection.h"
#include "../backend/Rest_Connection.h"
#include "../backend/Rest_Sink.h"
#include "../backend/BackendConnectionInterface.h"
#include "../backend/BackendSinkInterface.h"
#include "../backend/Local_Sink.h"
#include "../backend/RestGlobalLockService.h"
#include "../backend/Rest_Source.h"
#include "../backend/BackendConnectionManager.h"
#include "../backend/BackendSourceInterface.h"
#include "../backend/Local_Source.h"
#include "../backend/Rest_HttpHeadersWriter.h"
#include "../backend/BackendException.h"
#include "../backend/Buchla_Connection.h"
#include "../backend/ManagedBackendSink.h"
#include "../backend/BackendInterface.h"
#include "../backend/ManagedBackendSource.h"
#include "../backend/Rest_MetaData.h"

#endif // VOLUMEDRIVER_HPP_
