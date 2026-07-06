/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "impl/common/BLEGuards.h"
#if BLE_L2CAP_SUPPORTED

#include "BLEL2CAP.h"
#include <memory>

/**
 * @brief Backend-agnostic factory for @c BLEL2CAPChannel public handles.
 *
 * The channel's handle constructor is private; backend L2CAP server code
 * (@c BLEL2CAPServer::Impl, in impl/<stack>/) needs to mint channel handles from a
 * @c BLEL2CAPChannel::Impl when dispatching accept/data/disconnect events. @c BLEL2CAPServer::Impl
 * is nested in @c BLEL2CAPServer, not @c BLEL2CAPChannel, so it cannot reach that private ctor
 * directly. Routing through this friended, agnostically-named factory grants that access while
 * keeping any backend type out of the public header (mirrors the @c *ImplCommon::makeHandle pattern).
 */
struct BLEL2CAPChannelImplCommon {
  static BLEL2CAPChannel makeHandle(std::shared_ptr<BLEL2CAPChannel::Impl> impl) {
    return BLEL2CAPChannel(std::move(impl));
  }
};

#endif /* BLE_L2CAP_SUPPORTED */
