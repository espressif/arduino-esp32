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

#include "Arduino.h"
#include "vfs_api.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

using namespace fs;

#define DEFAULT_FILE_BUFFER_SIZE 4096

FileImplPtr VFSImpl::open(const char *fpath, const char *mode, const bool create) {
  FSLockGuard lock(_mtx);
  if (!_mountpoint) {
    log_e("File system is not mounted");
    return FileImplPtr();
  }

  if (!fpath || fpath[0] != '/') {
    log_e("%s does not start with /", fpath);
    return FileImplPtr();
  }

  size_t tempLen = strlen(_mountpoint) + strlen(fpath) + 1;
  char *temp = (char *)malloc(tempLen);
  if (!temp) {
    log_e("malloc failed");
    return FileImplPtr();
  }

  snprintf(temp, tempLen, "%s%s", _mountpoint, fpath);

  // Try to open as file first - let the file operation handle errors
  if (mode && mode[0] != 'r') {
    // For write modes, attempt to create directories if needed
    if (create) {
      char *token;
      char *folder = (char *)malloc(strlen(fpath) + 1);

      int start_index = 0;
      int end_index = 0;

      token = strchr(fpath + 1, '/');
      end_index = (token - fpath);

      while (token != NULL) {
        memcpy(folder, fpath + start_index, end_index - start_index);
        folder[end_index - start_index] = '\0';

        if (!VFSImpl::mkdir(folder)) {
          log_e("Creating folder: %s failed!", folder);
          free(folder);
          free(temp);
          return FileImplPtr();
        }

        token = strchr(token + 1, '/');
        if (token != NULL) {
          end_index = (token - fpath);
          memset(folder, 0, strlen(folder));
        }
      }

      free(folder);
    }

    // Try to open the file directly - let fopen handle errors
    free(temp);
    return std::make_shared<VFSFileImpl>(this, fpath, mode);
  }

  // For read mode, let the VFSFileImpl constructor handle the file opening
  // This avoids the TOCTOU race condition while maintaining proper functionality
  free(temp);
  return std::make_shared<VFSFileImpl>(this, fpath, mode);
}

bool VFSImpl::exists(const char *fpath) {
  FSLockGuard lock(_mtx);
  if (!_mountpoint) {
    log_e("File system is not mounted");
    return false;
  }

  VFSFileImpl f(this, fpath, "r");
  if (f) {
    f.close();
    return true;
  }
  return false;
}

bool VFSImpl::rename(const char *pathFrom, const char *pathTo) {
  FSLockGuard lock(_mtx);
  if (!_mountpoint) {
    log_e("File system is not mounted");
    return false;
  }

  if (!pathFrom || pathFrom[0] != '/' || !pathTo || pathTo[0] != '/') {
    log_e("bad arguments");
    return false;
  }

  size_t mountpointLen = strlen(_mountpoint);
  size_t temp1Len = mountpointLen + strlen(pathFrom) + 1;
  char *temp1 = (char *)malloc(temp1Len);
  if (!temp1) {
    log_e("malloc failed");
    return false;
  }
  size_t temp2Len = mountpointLen + strlen(pathTo) + 1;
  char *temp2 = (char *)malloc(temp2Len);
  if (!temp2) {
    free(temp1);
    log_e("malloc failed");
    return false;
  }

  snprintf(temp1, temp1Len, "%s%s", _mountpoint, pathFrom);
  snprintf(temp2, temp2Len, "%s%s", _mountpoint, pathTo);

  // Let rename() handle the error if source doesn't exist
  auto rc = ::rename(temp1, temp2);
  free(temp1);
  free(temp2);
  return rc == 0;
}

bool VFSImpl::remove(const char *fpath) {
  FSLockGuard lock(_mtx);
  if (!_mountpoint) {
    log_e("File system is not mounted");
    return false;
  }

  if (!fpath || fpath[0] != '/') {
    log_e("bad arguments");
    return false;
  }

  size_t tempLen = strlen(_mountpoint) + strlen(fpath) + 1;
  char *temp = (char *)malloc(tempLen);
  if (!temp) {
    log_e("malloc failed");
    return false;
  }

  snprintf(temp, tempLen, "%s%s", _mountpoint, fpath);

  // Let unlink() handle the error if file doesn't exist
  auto rc = unlink(temp);
  free(temp);
  return rc == 0;
}

bool VFSImpl::mkdir(const char *fpath) {
  FSLockGuard lock(_mtx);
  if (!_mountpoint) {
    log_e("File system is not mounted");
    return false;
  }

  size_t tempLen = strlen(_mountpoint) + strlen(fpath) + 1;
  char *temp = (char *)malloc(tempLen);
  if (!temp) {
    log_e("malloc failed");
    return false;
  }

  snprintf(temp, tempLen, "%s%s", _mountpoint, fpath);

  // Note: the stat() → opendir() → ::mkdir() sequence below contains a
  // TOCTOU (time-of-check / time-of-use) window between individual syscalls.
  // The per-filesystem mutex (_mtx) held by the caller (VFSImpl::mkdir) ensures
  // that no other Arduino FS API call on the same filesystem can interleave
  // here.  Code that bypasses the Arduino FS layer (direct fopen/stat/mkdir
  // calls) is not covered by this mutex.

  // stat() reports the path type without consuming a file descriptor.
  struct stat st;
  if (stat(temp, &st) == 0) {
    free(temp);
    if (S_ISDIR(st.st_mode)) {
      //log_w("%s already exists", fpath);
      return true;
    }
    log_e("%s is a file", fpath);
    return false;
  }

  // stat() failed (path does not exist).  On SPIFFS the filesystem is flat and
  // virtual directories are always reachable via opendir() regardless of
  // whether any files with that path prefix exist yet.  A successful opendir()
  // here means the path is an accessible virtual directory – treat it as
  // already present so we avoid calling ::mkdir() (which SPIFFS does not
  // support and would return an error).
  DIR *d = opendir(temp);
  if (d) {
    closedir(d);
    free(temp);
    return true;
  }

  auto rc = ::mkdir(temp, ACCESSPERMS);
  free(temp);
  return rc == 0;
}

bool VFSImpl::rmdir(const char *fpath) {
  FSLockGuard lock(_mtx);
  if (!_mountpoint) {
    log_e("File system is not mounted");
    return false;
  }

  if (strcmp(_mountpoint, "/spiffs") == 0) {
    log_e("rmdir is unnecessary in SPIFFS");
    return false;
  }

  size_t tempLen = strlen(_mountpoint) + strlen(fpath) + 1;
  char *temp = (char *)malloc(tempLen);
  if (!temp) {
    log_e("malloc failed");
    return false;
  }

  snprintf(temp, tempLen, "%s%s", _mountpoint, fpath);

  // Let rmdir() handle the error if directory doesn't exist
  auto rc = ::rmdir(temp);
  free(temp);
  return rc == 0;
}

VFSFileImpl::VFSFileImpl(VFSImpl *fs, const char *fpath, const char *mode) : _fs(fs), _f(NULL), _d(NULL), _path(NULL), _isDirectory(false), _written(false) {
  size_t tempLen = strlen(_fs->_mountpoint) + strlen(fpath) + 1;
  char *temp = (char *)malloc(tempLen);
  if (!temp) {
    return;
  }

  snprintf(temp, tempLen, "%s%s", _fs->_mountpoint, fpath);

  _path = strdup(fpath);
  if (!_path) {
    log_e("strdup(%s) failed", fpath);
    free(temp);
    return;
  }

  // For read mode, detect whether the path is a directory or a regular file.
  // The per-filesystem mutex held by VFSImpl::open serialises this constructor
  // against all other Arduino FS API calls on the same filesystem, so the
  // fopen/stat/opendir sequence is free of TOCTOU races from concurrent Arduino
  // FS callers.  Code that bypasses the Arduino FS layer (direct fopen/stat
  // calls) is not covered by the mutex.
  if (!mode || mode[0] == 'r') {
    _f = fopen(temp, mode);
    if (_f) {
      // fopen() succeeded — but on POSIX-style VFS layers (including ESP-IDF's
      // LittleFS VFS), open(2) can succeed on a directory with O_RDONLY.  Use
      // stat to confirm the path is a regular file before treating it as one.
      struct stat type_stat;
      if (stat(temp, &type_stat) == 0 && S_ISDIR(type_stat.st_mode)) {
        // Path is a directory; close the file handle and open as a directory.
        fclose(_f);
        _f = NULL;
        _d = opendir(temp);
        _isDirectory = (_d != NULL);
      } else {
        _isDirectory = false;
        if (!stat(temp, &_stat) && (_stat.st_blksize == 0)) {
          setvbuf(_f, NULL, _IOFBF, DEFAULT_FILE_BUFFER_SIZE);
        }
      }
    } else {
      // fopen failed — check via stat whether this is a directory.
      // stat() does not consume a file descriptor, so it can succeed even when
      // the filesystem's open-file limit has been reached.  If stat reports a
      // regular file we skip opendir() so an exhausted fd pool is not
      // misidentified as a directory.  If stat itself fails (as it does for
      // SPIFFS virtual directories which have no named entry of their own) we
      // still try opendir() because SPIFFS virtual directories are only
      // reachable that way.
      struct stat dir_stat;
      int stat_rc = stat(temp, &dir_stat);
      bool statIsFile = (stat_rc == 0 && !S_ISDIR(dir_stat.st_mode));
      if (!statIsFile) {
        _d = opendir(temp);
        _isDirectory = (_d != NULL);
      }
    }
  } else {
    //lets create this new file
    _isDirectory = false;
    _f = fopen(temp, mode);
    if (!_f) {
      log_e("fopen(%s) failed", temp);
    }
    if (!stat(temp, &_stat)) {
      if (_f && (_stat.st_blksize == 0)) {
        setvbuf(_f, NULL, _IOFBF, DEFAULT_FILE_BUFFER_SIZE);
      }
    } else {
      log_e("stat(%s) failed", temp);
    }
  }
  free(temp);
}

VFSFileImpl::~VFSFileImpl() {
  close();
}

void VFSFileImpl::close() {
  if (_path) {
    free(_path);
    _path = NULL;
  }
  if (_isDirectory && _d) {
    closedir(_d);
    _d = NULL;
    _isDirectory = false;
  } else if (_f) {
    fclose(_f);
    _f = NULL;
  }
}

VFSFileImpl::operator bool() {
  return (_isDirectory && _d != NULL) || _f != NULL;
}

time_t VFSFileImpl::getLastWrite() {
  _getStat();
  return _stat.st_mtime;
}

void VFSFileImpl::_getStat() const {
  if (!_path) {
    return;
  }
  size_t tempLen = strlen(_fs->_mountpoint) + strlen(_path) + 1;
  char *temp = (char *)malloc(tempLen);
  if (!temp) {
    return;
  }

  snprintf(temp, tempLen, "%s%s", _fs->_mountpoint, _path);

  if (!stat(temp, &_stat)) {
    _written = false;
  }
  free(temp);
}

size_t VFSFileImpl::write(const uint8_t *buf, size_t size) {
  if (_isDirectory || !_f || !buf || !size) {
    return 0;
  }
  _written = true;
  return fwrite(buf, 1, size, _f);
}

size_t VFSFileImpl::read(uint8_t *buf, size_t size) {
  if (_isDirectory || !_f || !buf || !size) {
    return 0;
  }

  return fread(buf, 1, size, _f);
}

void VFSFileImpl::flush() {
  if (_isDirectory || !_f) {
    return;
  }
  fflush(_f);
  // workaround for https://github.com/espressif/arduino-esp32/issues/1293
  fsync(fileno(_f));
}

bool VFSFileImpl::seek(uint32_t pos, SeekMode mode) {
  if (_isDirectory || !_f) {
    return false;
  }
  auto rc = fseek(_f, pos, mode);
  return rc == 0;
}

size_t VFSFileImpl::position() const {
  if (_isDirectory || !_f) {
    return 0;
  }
  return ftell(_f);
}

size_t VFSFileImpl::size() const {
  if (_isDirectory || !_f) {
    return 0;
  }
  if (_written) {
    _getStat();
  }
  return _stat.st_size;
}

/*
* Change size of files internal buffer used for read / write operations.
* Need to be called right after opening file before any other operation!
*/
bool VFSFileImpl::setBufferSize(size_t size) {
  if (_isDirectory || !_f) {
    return 0;
  }
  int res = setvbuf(_f, NULL, _IOFBF, size);
  return res == 0;
}

const char *VFSFileImpl::path() const {
  return (const char *)_path;
}

const char *VFSFileImpl::name() const {
  return pathToFileName(path());
}

//to implement
boolean VFSFileImpl::isDirectory(void) {
  return _isDirectory;
}

FileImplPtr VFSFileImpl::openNextFile(const char *mode) {
  if (!_isDirectory || !_d) {
    return FileImplPtr();
  }
  struct dirent *file = readdir(_d);
  if (file == NULL) {
    return FileImplPtr();
  }
  if (file->d_type != DT_REG && file->d_type != DT_DIR) {
    return openNextFile(mode);
  }

  size_t pathLen = strlen(_path);
  size_t fileNameLen = strlen(file->d_name);
  size_t nameLen = pathLen + fileNameLen + 2;
  char *name = (char *)malloc(nameLen);

  if (name == NULL) {
    return FileImplPtr();
  }

  if ((file->d_name[0] != '/') && (_path[pathLen - 1] != '/')) {
    snprintf(name, nameLen, "%s/%s", _path, file->d_name);
  } else {
    snprintf(name, nameLen, "%s%s", _path, file->d_name);
  }

  FileImplPtr fileImplPtr = std::make_shared<VFSFileImpl>(_fs, name, mode);
  free(name);
  return fileImplPtr;
}

boolean VFSFileImpl::seekDir(long position) {
  if (!_d) {
    return false;
  }
  seekdir(_d, position);
  return true;
}

String VFSFileImpl::getNextFileName() {
  if (!_isDirectory || !_d) {
    return "";
  }
  struct dirent *file = readdir(_d);
  if (file == NULL) {
    return "";
  }
  if (file->d_type != DT_REG && file->d_type != DT_DIR) {
    return "";
  }
  String fname = String(file->d_name);
  String name = String(_path);
  if (!fname.startsWith("/") && !name.endsWith("/")) {
    name += "/";
  }
  name += fname;
  return name;
}

String VFSFileImpl::getNextFileName(bool *isDir) {
  if (!_isDirectory || !_d) {
    return "";
  }
  struct dirent *file = readdir(_d);
  if (file == NULL) {
    return "";
  }
  if (file->d_type != DT_REG && file->d_type != DT_DIR) {
    return "";
  }
  String fname = String(file->d_name);
  String name = String(_path);
  if (!fname.startsWith("/") && !name.endsWith("/")) {
    name += "/";
  }
  name += fname;

  // check entry is a directory
  if (isDir) {
    *isDir = (file->d_type == DT_DIR);
  }
  return name;
}

void VFSFileImpl::rewindDirectory(void) {
  if (!_isDirectory || !_d) {
    return;
  }
  rewinddir(_d);
}
