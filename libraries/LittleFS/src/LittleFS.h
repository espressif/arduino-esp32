// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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
#ifndef _LITTLEFS_H_
#define _LITTLEFS_H_

#include "FS.h"

namespace fs {

class LittleFSFS : public FS {
public:
  LittleFSFS();
  ~LittleFSFS();
  bool begin(bool formatOnFail = false, const char *basePath = "/littlefs", uint8_t maxOpenFiles = 10, const char *partitionLabel = "spiffs");
  bool format();
  size_t totalBytes();
  size_t usedBytes();
  void end();

private:
  char *partitionLabel_;
};

}  // namespace fs

extern fs::LittleFSFS LittleFS;

#endif
