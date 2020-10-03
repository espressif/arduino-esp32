#ifndef REQUESTHANDLERSIMPL_H
#define REQUESTHANDLERSIMPL_H

#include "RequestHandler.h"
#include "mimetable.h"
#include "WString.h"
#include "Uri.h"

using namespace mime;

class FunctionRequestHandler : public RequestHandler {
public:
    FunctionRequestHandler(WebServer::THandlerFunction fn, WebServer::THandlerFunction ufn, const Uri &uri, HTTPMethod method)
    : _fn(fn)
    , _ufn(ufn)
    , _uri(uri.clone())
    , _method(method)
    {
        _uri->initPathArgs(pathArgs);
    }

    ~FunctionRequestHandler() {
        delete _uri;
    }

    bool canHandle(HTTPMethod requestMethod, String requestUri) override  {
        if (_method != HTTP_ANY && _method != requestMethod)
            return false;

        return _uri->canHandle(requestUri, pathArgs);
    }

    bool canUpload(String requestUri) override  {
        if (!_ufn || !canHandle(HTTP_POST, requestUri))
            return false;

        return true;
    }

    bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) override {
        (void) server;
        if (!canHandle(requestMethod, requestUri))
            return false;

        _fn();
        return true;
    }

    void upload(WebServer& server, String requestUri, HTTPUpload& upload) override {
        (void) server;
        (void) upload;
        if (canUpload(requestUri))
            _ufn();
    }

protected:
    WebServer::THandlerFunction _fn;
    WebServer::THandlerFunction _ufn;
    Uri *_uri;
    HTTPMethod _method;
};

class StaticRequestHandler : public RequestHandler {
public:
    StaticRequestHandler(FS& fs, const char* path, const char* uri, const char* cache_header)
    : _fs(fs)
    , _uri(uri)
    , _path(path)
    , _cache_header(cache_header)
    {
        _isFile = fs.exists(path);
        log_v("StaticRequestHandler: path=%s uri=%s isFile=%d, cache_header=%s\r\n", path, uri, _isFile, cache_header);
        _baseUriLength = _uri.length();
    }

    bool canHandle(HTTPMethod requestMethod, String requestUri) override  {
        if (requestMethod != HTTP_GET)
            return false;

        if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri))
            return false;

        return true;
    }

    bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) override {
        if (!canHandle(requestMethod, requestUri))
            return false;

        log_v("StaticRequestHandler::handle: request=%s _uri=%s\r\n", requestUri.c_str(), _uri.c_str());

        String path(_path);

        if (!_isFile) {
            // Base URI doesn't point to a file.
            // If a directory is requested, look for index file.
            if (requestUri.endsWith("/")) 
              requestUri += "index.htm";

            // Append whatever follows this URI in request to get the file path.
            path += requestUri.substring(_baseUriLength);
        }
        log_v("StaticRequestHandler::handle: path=%s, isFile=%d\r\n", path.c_str(), _isFile);

        String contentType = getContentType(path);

        // look for gz file, only if the original specified path is not a gz.  So part only works to send gzip via content encoding when a non compressed is asked for
        // if you point the the path to gzip you will serve the gzip as content type "application/x-gzip", not text or javascript etc...
        if (!path.endsWith(FPSTR(mimeTable[gz].endsWith)) && !_fs.exists(path))  {
            String pathWithGz = path + FPSTR(mimeTable[gz].endsWith);
            if(_fs.exists(pathWithGz))
                path += FPSTR(mimeTable[gz].endsWith);
        }

        File f = _fs.open(path, "r");
        if (!f || !f.available())
            return false;

        if (_cache_header.length() != 0)
            server.sendHeader("Cache-Control", _cache_header);

        server.streamFile(f, contentType);
        return true;
    }

    static String getContentType(const String& path) {
        char buff[sizeof(mimeTable[0].mimeType)];
        // Check all entries but last one for match, return if found
        for (size_t i=0; i < sizeof(mimeTable)/sizeof(mimeTable[0])-1; i++) {
            strcpy_P(buff, mimeTable[i].endsWith);
            if (path.endsWith(buff)) {
                strcpy_P(buff, mimeTable[i].mimeType);
                return String(buff);
            }
        }
        // Fall-through and just return default type
        strcpy_P(buff, mimeTable[sizeof(mimeTable)/sizeof(mimeTable[0])-1].mimeType);
        return String(buff);
    }

protected:
    FS _fs;
    String _uri;
    String _path;
    String _cache_header;
    bool _isFile;
    size_t _baseUriLength;
};


#endif //REQUESTHANDLERSIMPL_H
