// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#include "HashBuilder.h"

void HashBuilder::add(const char *data) {
  add((const uint8_t *)data, strlen(data));
}

void HashBuilder::add(String data) {
  add(data.c_str());
}

void HashBuilder::addHexString(const char *data) {
  size_t len = strlen(data);
  uint8_t *tmp = (uint8_t *)malloc(len / 2);
  if (tmp == NULL) {
    return;
  }
  hex2bytes(tmp, len / 2, data);
  add(tmp, len / 2);
  free(tmp);
}

void HashBuilder::addHexString(String data) {
  addHexString(data.c_str());
}
