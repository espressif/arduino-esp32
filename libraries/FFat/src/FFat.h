// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#ifndef _FFAT_H_
#define _FFAT_H_

#include "FS.h"
#include "wear_levelling.h"

#define FFAT_WIPE_QUICK      0
#define FFAT_WIPE_FULL       1
#define FFAT_PARTITION_LABEL "ffat"

namespace fs {

class F_Fat : public FS {
public:
  F_Fat(FSImplPtr impl);
  bool begin(bool formatOnFail = false, const char *basePath = "/ffat", uint8_t maxOpenFiles = 10, const char *partitionLabel = (char *)FFAT_PARTITION_LABEL);
  bool format(bool full_wipe = FFAT_WIPE_QUICK, char *partitionLabel = (char *)FFAT_PARTITION_LABEL);
  size_t totalBytes();
  size_t usedBytes();
  size_t freeBytes();
  void end();

private:
  wl_handle_t _wl_handle = WL_INVALID_HANDLE;
};

}  // namespace fs

extern fs::F_Fat FFat;

#endif /* _FFAT_H_ */
