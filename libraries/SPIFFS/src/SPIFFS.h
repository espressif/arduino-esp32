#pragma once

#include "FS.h"
#include "FSImpl.h"
#include "vfs_api.h"

namespace fs
{

class SPIFFSImpl : public VFSImpl
{
public:
	SPIFFSImpl();
    virtual ~SPIFFSImpl() { }
    virtual bool exists(const char* path);
};

class SPIFFSFS : public FS
{
public:
	SPIFFSFS();
    bool begin(bool formatOnFail=false, const char * basePath="/spiffs", uint8_t maxOpenFiles=10);
    bool format();
    size_t totalBytes();
    size_t usedBytes();
    void end();
};

}

extern fs::SPIFFSFS SPIFFS;


