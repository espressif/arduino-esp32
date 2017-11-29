#include "FSPreferences.h"

enum FSItemType : uint8_t {
    ERR = 0,
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
    FLO,
    DOB,
    BOO,
    STR,
    BLOB    
};

FSPreferences::FSPreferences(fs::FS &_handle) : _handle(_handle) {
    //this._handle = _handle;
    _namespacePath = nullptr;
    _filePath = nullptr;
    _started = false;
}

FSPreferences::~FSPreferences(){
    end();
}

bool FSPreferences::begin(const char * name, bool readOnly){
    if(_started){
        return false;
    }
    _started = true;
    _readOnly = readOnly;

    const char * base = "/FSPreferences";

    if(!_handle.mkdir(base)) {
        log_e("mkdir '%s' failed!", base);
        end();
        return false;
    }

    _namespacePath = (char *)malloc( strlen(base) + strlen(name) + 2 );
    
    if(!_namespacePath) {
        log_e("malloc failed!");
        end();
        return false;
    }

    sprintf(_namespacePath,"%s/%s", base, name);

    if(!_handle.mkdir(_namespacePath)) {
        log_e("mkdir '%s' failed!", _namespacePath);
        end();
        return false;
    }

    return true;
}

void FSPreferences::end(){
    if(!_started){
        return;
    }
    free(_namespacePath);
    _namespacePath = nullptr;
    free(_filePath);
    _filePath = nullptr;
    _started = false;
}

bool FSPreferences::clear() {
    if(!_started || _readOnly){
        return false;
    }

    File root = _handle.open(_namespacePath);
    if(!root || !root.isDirectory()){
        log_e("failed to open namespace directory!");
        return false;
    }

    bool success;
    File file = root.openNextFile();
    while(file){
        String filePath = String(file.name());
        bool isFile = !file.isDirectory();
        file.close();

        
        if(isFile){
            success = _handle.remove(filePath.c_str());
        } else {
            success = _handle.rmdir(filePath.c_str());
        }
        
        if(!success){
            log_e("there was an error deleting namespace entry '%s'!", filePath.c_str());
            break;
        }
        
        file = root.openNextFile();
    }
    root.close();

    return success;
}

bool FSPreferences::setFilePath(const char * key) {
    if(!_namespacePath || !key){
        log_e("key and namespace paths must not be NULL");
        free(_filePath);
        _filePath = nullptr;
        return false;
    }

    char * tempFilePath = (char *)realloc(_filePath, strlen(_namespacePath) + strlen(key) + 2 );
    if(tempFilePath == nullptr) {
        free(_filePath);
        _filePath = nullptr;
        log_e("realloc failed!");        
    } else {
        sprintf(tempFilePath,"%s/%s", _namespacePath, key);
    }
    _filePath = tempFilePath;

    return (_filePath != nullptr);
}

bool FSPreferences::remove(const char * key) {
    if(!_started || !key || _readOnly){
        return false;
    }

    if(!setFilePath(key)) {
        return false;
    }

    if(!_handle.exists(_filePath)) {
        log_e("key does not exist!"); 
        return false;
    }

    if(!_handle.remove(_filePath)) {
        log_e("key could not be removed!"); 
        return false;
    }

    return true;
}

size_t FSPreferences::putBytes(const char* key, uint8_t type, const void* value, size_t len) {
    if(!_started || !key || _readOnly || !type || !len){
        return 0;
    }

    if(!setFilePath(key)) {
        return 0;
    }

    if(_handle.exists(_filePath)) {
        File existing = _handle.open(_filePath, FILE_READ);

        if(!existing) {
            log_e("existing file '%s' could not be opened!", _filePath); 
            return 0;
        }

        if(existing.isDirectory()) {
            log_e("existing file '%s' is a directory!", _filePath); 
            existing.close();
            return 0;
        }

        uint8_t existingType = existing.available() ? (FSItemType)existing.peek() : FSItemType::ERR;
        existing.close();

        if(existingType != type) {
            log_e("existing file '%s' type is: %u, expected: %u", _filePath, existingType, type); 
            return 0;
        }
    }

    File newItem = _handle.open(_filePath, FILE_WRITE);

    if(!newItem) {
        log_e("new file '%s' could not be opened!", _filePath); 
        return 0;
    }

    if(!newItem.write(type) || !newItem.write((uint8_t*)value, len)) {
        log_e("new file '%s' could not be written to!", _filePath); 
        newItem.close();
        return 0;
    }

    newItem.close();
    return len;
}

size_t FSPreferences::getBytes(const char* key, uint8_t type, void* buf, size_t maxLen, size_t minLen) {
    if(!_started || !key || !type || !maxLen){
        return 0;
    }

    if(!setFilePath(key)) {
        return 0;
    }

    if(!_handle.exists(_filePath)) {
        log_w("Key '%s' could not be found at '%s'", key, _filePath); 
        return 0;
    }

    File existing = _handle.open(_filePath, FILE_READ);
    
    if(!existing) {
        log_e("existing file '%s' could not be opened!", _filePath); 
        return 0;
    }

    if(existing.isDirectory()) {
        log_e("existing file '%s' is a directory!", _filePath); 
        existing.close();
        return 0;
    }

    size_t fileSize = existing.size();

    if(fileSize < minLen + 1) {
        log_e("existing file '%s' size is: %zu, expected minimum: %zu", _filePath, fileSize, minLen + 1); 
        existing.close();
        return 0;
    }    

    uint8_t existingType = existing.available() ? (FSItemType)existing.read() : FSItemType::ERR;

    if(existingType != type) {
        log_e("existing file '%s' type is: %u, expected: %u", _filePath, existingType, type); 
        existing.close();
        return 0;
    }

    size_t recordSize = fileSize - 1;
    const size_t len = maxLen ? std::min(recordSize, maxLen) : recordSize;

    char data[len]{0};

    const size_t resLen = existing.read((uint8_t*)data, len);
    existing.close();

    if (resLen != len) {
        log_e("existing file '%s' read  '%zu' bytes, expected '%zu' ", _filePath, resLen, len); 
        return 0;
    }

    memcpy(buf, data, len);
    return len;
}

size_t FSPreferences::putChar(const char* key, int8_t value) {
    return putBytes(key, FSItemType::I8, &value, sizeof(value));
}

size_t FSPreferences::putUChar(const char* key, uint8_t value) {
    return putBytes(key, FSItemType::U8, &value, sizeof(value));
}

size_t FSPreferences::putShort(const char* key, int16_t value) {
    return putBytes(key, FSItemType::I16, &value, sizeof(value));
}

size_t FSPreferences::putUShort(const char* key, uint16_t value) {
    return putBytes(key, FSItemType::U16, &value, sizeof(value));
}

size_t FSPreferences::putInt(const char* key, int32_t value) {
    return putBytes(key, FSItemType::I32, &value, sizeof(value));
}

size_t FSPreferences::putUInt(const char* key, uint32_t value) {
    return putBytes(key, FSItemType::U32, &value, sizeof(value));
}

size_t FSPreferences::putLong(const char* key, int32_t value) {
    return putBytes(key, FSItemType::I32, &value, sizeof(value));
}

size_t FSPreferences::putULong(const char* key, uint32_t value) {
    return putBytes(key, FSItemType::U32, &value, sizeof(value));
}

size_t FSPreferences::putLong64(const char* key, int64_t value) {
    return putBytes(key, FSItemType::I64, &value, sizeof(value));
}

size_t FSPreferences::putULong64(const char* key, uint64_t value) {
    return putBytes(key, FSItemType::U64, &value, sizeof(value));
}

size_t FSPreferences::putFloat(const char* key, float_t value) {
    return putBytes(key, FSItemType::FLO, &value, sizeof(value));
}

size_t FSPreferences::putDouble(const char* key, double_t value) {
    return putBytes(key, FSItemType::DOB, &value, sizeof(value));
}

size_t FSPreferences::putBool(const char* key, bool value) {
    return putBytes(key, FSItemType::BOO, &value, sizeof(value));
}

size_t FSPreferences::putString(const char* key, const char* value) {
    return putBytes(key, FSItemType::STR, value, strlen(value) + 1);
}

size_t FSPreferences::putString(const char* key, const String& value) {
    return putString(key, value.c_str());
}

size_t FSPreferences::putBytes(const char* key, const void* value, size_t len) {
    return putBytes(key, FSItemType::BLOB, value, len);
}

int8_t FSPreferences::getChar(const char* key, int8_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::I8, (void*) &result, len, len);
    return result;
}

uint8_t FSPreferences::getUChar(const char* key, uint8_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::U8, (void*) &result, len, len);
    return result;
}

int16_t FSPreferences::getShort(const char* key, int16_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::I16, (void*) &result, len, len);
    return result;
}

uint16_t FSPreferences::getUShort(const char* key, uint16_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::U16, (void*) &result, len, len);
    return result;
}

int32_t FSPreferences::getInt(const char* key, int32_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::I32, (void*) &result, len, len);
    return result;
}

uint32_t FSPreferences::getUInt(const char* key, uint32_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::U32, (void*) &result, len, len);
    return result;
}

int32_t FSPreferences::getLong(const char* key, int32_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::I32, (void*) &result, len, len);
    return result;
}

uint32_t FSPreferences::getULong(const char* key, uint32_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::U32, (void*) &result, len, len);
    return result;
}

int64_t FSPreferences::getLong64(const char* key, int64_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::I64, (void*) &result, len, len);
    return result;
}

uint64_t FSPreferences::getULong64(const char* key, uint64_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::U64, (void*) &result, len, len);
    return result;
}

float_t FSPreferences::getFloat(const char* key, float_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::FLO, (void*) &result, len, len);
    return result;
}

double_t FSPreferences::getDouble(const char* key, double_t defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::DOB, (void*) &result, len, len);
    return result;
}

bool FSPreferences::getBool(const char* key, bool defaultValue) {
    auto result = defaultValue;
    size_t len = sizeof(defaultValue);
    getBytes(key, FSItemType::BOO, (void*) &result, len, len);
    return result;
}

size_t FSPreferences::getBytes(const char* key, void * buf, size_t maxLen) {
    return getBytes(key, FSItemType::BLOB, buf, maxLen);
}


size_t FSPreferences::getString(const char* key, char* value, size_t maxLen) {    
    return getBytes(key, FSItemType::STR, (void*) value, maxLen);
}

String FSPreferences::getString(const char* key, String defaultValue) {
    if(!_started || !key){
        return defaultValue;
    }

    if(!setFilePath(key)) {
        return defaultValue;
    }

    if(!_handle.exists(_filePath)) {
        log_w("Key '%s' could not be found at '%s'", key, _filePath); 
        return defaultValue;
    }

    File existing = _handle.open(_filePath, FILE_READ);
    
    if(!existing) {
        log_e("existing file '%s' could not be opened!", _filePath); 
        return defaultValue;
    }

    if(existing.isDirectory()) {
        log_e("existing file '%s' is a directory!", _filePath); 
        existing.close();
        return defaultValue;
    }

    size_t fileSize = existing.size();

    if(fileSize < 2) {
        log_e("existing file '%s' size is: %zu, expected minimum: 2", _filePath, fileSize); 
        existing.close();
        return defaultValue;
    }    

    uint8_t existingType = existing.available() ? (FSItemType)existing.read() : FSItemType::ERR;

    if(existingType != FSItemType::STR) {
        log_e("existing file '%s' type is: %u, expected: %u", _filePath, existingType, FSItemType::STR); 
        existing.close();
        return defaultValue;
    }

    size_t recordSize = fileSize - 1;

    char data[recordSize + 1]{0};

    const size_t resLen = existing.read((uint8_t*)data, recordSize);
    existing.close();

    if (resLen != recordSize) {
        log_e("existing file '%s' read  '%zu' bytes, expected '%zu' ", _filePath, resLen, recordSize); 
        return defaultValue;
    }

    return String(data);
}