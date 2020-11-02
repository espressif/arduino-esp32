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

#include "vfs_api.h"

using namespace fs;

FileImplPtr VFSImpl::open(const char* path, const char* mode)
{
    if(!_mountpoint) {
        log_e("File system is not mounted");
        return FileImplPtr();
    }

    if(!path || path[0] != '/') {
        log_e("%s does not start with /", path);
        return FileImplPtr();
    }

    char * temp = (char *)malloc(strlen(path)+strlen(_mountpoint)+2);
    if(!temp) {
        log_e("malloc failed");
        return FileImplPtr();
    }

    sprintf(temp,"%s%s", _mountpoint, path);

    struct stat st;
    //file lound
    if(!stat(temp, &st)) {
        free(temp);
        if (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) {
            return std::make_shared<VFSFileImpl>(this, path, mode);
        }
        log_e("%s has wrong mode 0x%08X", path, st.st_mode);
        return FileImplPtr();
    }

    //file not found but mode permits creation
    if(mode && mode[0] != 'r') {
        free(temp);
        return std::make_shared<VFSFileImpl>(this, path, mode);
    }

    //try to open this as directory (might be mount point)
    DIR * d = opendir(temp);
    if(d) {
        closedir(d);
        free(temp);
        return std::make_shared<VFSFileImpl>(this, path, mode);
    }

    log_e("%s does not exist", temp);
    free(temp);
    return FileImplPtr();
}

bool VFSImpl::exists(const char* path)
{
    if(!_mountpoint) {
        log_e("File system is not mounted");
        return false;
    }

    VFSFileImpl f(this, path, "r");
    if(f) {
        f.close();
        return true;
    }
    return false;
}

bool VFSImpl::rename(const char* pathFrom, const char* pathTo)
{
    if(!_mountpoint) {
        log_e("File system is not mounted");
        return false;
    }

    if(!pathFrom || pathFrom[0] != '/' || !pathTo || pathTo[0] != '/') {
        log_e("bad arguments");
        return false;
    }
    if(!exists(pathFrom)) {
        log_e("%s does not exists", pathFrom);
        return false;
    }
    char * temp1 = (char *)malloc(strlen(pathFrom)+strlen(_mountpoint)+1);
    if(!temp1) {
        log_e("malloc failed");
        return false;
    }
    char * temp2 = (char *)malloc(strlen(pathTo)+strlen(_mountpoint)+1);
    if(!temp2) {
        free(temp1);
        log_e("malloc failed");
        return false;
    }
    sprintf(temp1,"%s%s", _mountpoint, pathFrom);
    sprintf(temp2,"%s%s", _mountpoint, pathTo);
    auto rc = ::rename(temp1, temp2);
    free(temp1);
    free(temp2);
    return rc == 0;
}

bool VFSImpl::remove(const char* path)
{
    if(!_mountpoint) {
        log_e("File system is not mounted");
        return false;
    }

    if(!path || path[0] != '/') {
        log_e("bad arguments");
        return false;
    }

    VFSFileImpl f(this, path, "r");
    if(!f || f.isDirectory()) {
        if(f) {
            f.close();
        }
        log_e("%s does not exists or is directory", path);
        return false;
    }
    f.close();

    char * temp = (char *)malloc(strlen(path)+strlen(_mountpoint)+1);
    if(!temp) {
        log_e("malloc failed");
        return false;
    }
    sprintf(temp,"%s%s", _mountpoint, path);
    auto rc = unlink(temp);
    free(temp);
    return rc == 0;
}

bool VFSImpl::mkdir(const char *path)
{
    if(!_mountpoint) {
        log_e("File system is not mounted");
        return false;
    }

    VFSFileImpl f(this, path, "r");
    if(f && f.isDirectory()) {
        f.close();
        //log_w("%s already exists", path);
        return true;
    } else if(f) {
        f.close();
        log_e("%s is a file", path);
        return false;
    }

    char * temp = (char *)malloc(strlen(path)+strlen(_mountpoint)+1);
    if(!temp) {
        log_e("malloc failed");
        return false;
    }
    sprintf(temp,"%s%s", _mountpoint, path);
    auto rc = ::mkdir(temp, ACCESSPERMS);
    free(temp);
    return rc == 0;
}

bool VFSImpl::rmdir(const char *path)
{
    if(!_mountpoint) {
        log_e("File system is not mounted");
        return false;
    }

    if (strcmp(_mountpoint, "/spiffs") == 0) {
        log_e("rmdir is unnecessary in SPIFFS");
        return false;
    }

    VFSFileImpl f(this, path, "r");
    if(!f || !f.isDirectory()) {
        if(f) {
            f.close();
        }
        log_e("%s does not exists or is a file", path);
        return false;
    }
    f.close();

    char * temp = (char *)malloc(strlen(path)+strlen(_mountpoint)+1);
    if(!temp) {
        log_e("malloc failed");
        return false;
    }
    sprintf(temp,"%s%s", _mountpoint, path);
    auto rc = ::rmdir(temp);
    free(temp);
    return rc == 0;
}




VFSFileImpl::VFSFileImpl(VFSImpl* fs, const char* path, const char* mode)
    : _fs(fs)
    , _f(NULL)
    , _d(NULL)
    , _path(NULL)
    , _isDirectory(false)
    , _written(false)
{
    char * temp = (char *)malloc(strlen(path)+strlen(_fs->_mountpoint)+1);
    if(!temp) {
        return;
    }
    sprintf(temp,"%s%s", _fs->_mountpoint, path);

    _path = strdup(path);
    if(!_path) {
        log_e("strdup(%s) failed", path);
        free(temp);
        return;
    }

    if(!stat(temp, &_stat)) {
        //file found
        if (S_ISREG(_stat.st_mode)) {
            _isDirectory = false;
            _f = fopen(temp, mode);
            if(!_f) {
                log_e("fopen(%s) failed", temp);
            }
        } else if(S_ISDIR(_stat.st_mode)) {
            _isDirectory = true;
            _d = opendir(temp);
            if(!_d) {
                log_e("opendir(%s) failed", temp);
            }
        } else {
            log_e("Unknown type 0x%08X for file %s", ((_stat.st_mode)&_IFMT), temp);
        }
    } else {
        //file not found
        if(!mode || mode[0] == 'r') {
            //try to open as directory
            _d = opendir(temp);
            if(_d) {
                _isDirectory = true;
            } else {
                _isDirectory = false;
                //log_w("stat(%s) failed", temp);
            }
        } else {
            //lets create this new file
            _isDirectory = false;
            _f = fopen(temp, mode);
            if(!_f) {
                log_e("fopen(%s) failed", temp);
            }
        }
    }
    free(temp);
}

VFSFileImpl::~VFSFileImpl()
{
    close();
}

void VFSFileImpl::close()
{
    if(_path) {
        free(_path);
        _path = NULL;
    }
    if(_isDirectory && _d) {
        closedir(_d);
        _d = NULL;
        _isDirectory = false;
    } else if(_f) {
        fclose(_f);
        _f = NULL;
    }
}

VFSFileImpl::operator bool()
{
    return (_isDirectory && _d != NULL) || _f != NULL;
}

time_t VFSFileImpl::getLastWrite() {
    _getStat() ;
    return _stat.st_mtime;
}

void VFSFileImpl::_getStat() const
{
    if(!_path) {
        return;
    }
    char * temp = (char *)malloc(strlen(_path)+strlen(_fs->_mountpoint)+1);
    if(!temp) {
        return;
    }
    sprintf(temp,"%s%s", _fs->_mountpoint, _path);
    if(!stat(temp, &_stat)) {
        _written = false;
    }
    free(temp);
}

size_t VFSFileImpl::write(const uint8_t *buf, size_t size)
{
    if(_isDirectory || !_f || !buf || !size) {
        return 0;
    }
    _written = true;
    return fwrite(buf, 1, size, _f);
}

size_t VFSFileImpl::read(uint8_t* buf, size_t size)
{
    if(_isDirectory || !_f || !buf || !size) {
        return 0;
    }

    return fread(buf, 1, size, _f);
}

void VFSFileImpl::flush()
{
    if(_isDirectory || !_f) {
        return;
    }
    fflush(_f);
    // workaround for https://github.com/espressif/arduino-esp32/issues/1293
    fsync(fileno(_f));
}

bool VFSFileImpl::seek(uint32_t pos, SeekMode mode)
{
    if(_isDirectory || !_f) {
        return false;
    }
    auto rc = fseek(_f, pos, mode);
    return rc == 0;
}

size_t VFSFileImpl::position() const
{
    if(_isDirectory || !_f) {
        return 0;
    }
    return ftell(_f);
}

size_t VFSFileImpl::size() const
{
    if(_isDirectory || !_f) {
        return 0;
    }
    if (_written) {
        _getStat();
    }
    return _stat.st_size;
}

const char* VFSFileImpl::name() const
{
    return (const char*) _path;
}

//to implement
boolean VFSFileImpl::isDirectory(void)
{
    return _isDirectory;
}

FileImplPtr VFSFileImpl::openNextFile(const char* mode)
{
    if(!_isDirectory || !_d) {
        return FileImplPtr();
    }
    struct dirent *file = readdir(_d);
    if(file == NULL) {
        return FileImplPtr();
    }
    if(file->d_type != DT_REG && file->d_type != DT_DIR) {
        return openNextFile(mode);
    }
    String fname = String(file->d_name);
    String name = String(_path);
    if(!fname.startsWith("/") && !name.endsWith("/")) {
        name += "/";
    }
    name += fname;

    return std::make_shared<VFSFileImpl>(_fs, name.c_str(), mode);
}

void VFSFileImpl::rewindDirectory(void)
{
    if(!_isDirectory || !_d) {
        return;
    }
    rewinddir(_d);
}
