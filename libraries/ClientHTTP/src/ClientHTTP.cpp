/*
 *   ClientHTTP.h - A HTTP Client class using the standard Client and Stream interface,
 *                  while handling the request and response headers and chunked
 *                  Transfer-Encoding 'under the hood'
 *                  
 *   Copyright (C) 2020  Jeroen Döll
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
 
 #include "ClientHTTP.h"


                    ClientHTTP::ClientHTTP(Client   &client): _client(client) {
  _host = NULL;
  _state = CLIENT_READY;
  _location = NULL;
  _redirectCount = 0;
  strcpy(_originalMethod, "");
}


                    ClientHTTP::~ClientHTTP() {
  free(_location);

// TO DO RESPONSE HEADERS
//  for(auto responseHeader: responseHeaders) { 
//    free(responseHeader.second); // !!!!!! NOT SURE IF THESE POINTERS MUST BE FREE-ED
//  }

  free(_host);
}

int                 ClientHTTP::connect(IPAddress ip, uint16_t port) {
  if(_host) {
    free(_host);
    _host = NULL;
  }

  _ip = ip;
  _port = port;

#if defined(ESP32)
  log_v("Connect to %s at port %u", ip.toString().c_str(), port);
#endif

  return _client.connect(ip, port);
}

int                 ClientHTTP::connect(const char * host, uint16_t port) {
  _ip = INADDR_NONE;
  
  if(_host) {
    if(strcasecmp(_host,host) != 0) { // Only make necessary heap mutations
      free(_host);
      _host = strdup(host);  
    }
  } else {
    _host = strdup(host);
  }
  if(_host == NULL) {
#if defined(ESP32)
    log_e("Out of memory");
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("Out of memory");
#endif

    return 0;
  }

  _port = port;

#if defined(ESP32)
  log_v("Connect to %s at port %u", host, port);
#endif

  return _client.connect(host, port);
}

size_t              ClientHTTP::write(uint8_t c) {
  if(_state != CLIENT_REQUEST_HEADER_SENT) {
#if defined(ESP32)
    log_e("Invalid state: write");
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("Invalid state: write");
#endif

    return 0;
  }

  if(_leftToWrite == 0) {
#if defined(ESP32)
    log_v("No more bytes to write");
#endif

    return 0;
  }

  size_t bytesWritten = _client.write(c);
  _leftToWrite -= bytesWritten;
  if(_leftToWrite == 0) {
//    _client.flush();  // !!!!!!!! DOES THIS FLUSH THE WRITE BUFFER ONLY (SHOULD NOT FLUSH READ BUFFER)
    _state = CLIENT_RESPONSE_HEADER;
  }

  return bytesWritten;
}

size_t              ClientHTTP::write(const uint8_t *buf, size_t size) {
  if(_state != CLIENT_REQUEST_HEADER_SENT) {
#if defined(ESP32)
    log_e("Invalid state: write");
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("Invalid state: write");
#endif

    return 0;
  }

  if(_leftToWrite == 0) {
#if defined(ESP32)
    log_v("No more bytes to write");
#endif

    return 0;
  }

  size_t bytesToWrite = size;
  if(bytesToWrite > _leftToWrite) {
#if defined(ESP32)
    log_d("Too many bytes to write: truncated %u bytes", bytesToWrite - _leftToWrite);
#endif

    bytesToWrite = _leftToWrite;
  }

  size_t bytesWritten = _client.write(buf, bytesToWrite);
  if(_leftToWrite == 0) { // Chunked write, is a chunked request part of the HTTP 1.1 standard?
    if(bytesWritten != 0 || size == 0) {
      if(!_firstChunkToWrite) {
        _client.print("\r\n");
      } else {
        _firstChunkToWrite = false;      
      }
      _client.printf("%x\r\n", bytesWritten);
      if(size == 0) { // empty write to mark end of chunked write
        _state = CLIENT_RESPONSE_HEADER;      
      }
    }
  } else {
    _leftToWrite -= bytesWritten;
    if(_leftToWrite == 0) {
 //     _client.flush();  // !!!!!!!! DOES THIS FLUSH THE WRITE BUFFER ONLY (SHOULD NOT FLUSH READ BUFFER)
      _state = CLIENT_RESPONSE_HEADER;
    }
  }

  return bytesWritten;
}

int                 ClientHTTP::available() {
  if(_state == CLIENT_RESPONSE_PAYLOAD) {
    if(_leftToRead == 0) {
      if(_chunked) {
        char chunkHeader[7];
        size_t bytesRead = _client.readBytesUntil('\n', chunkHeader, sizeof chunkHeader);
        if(bytesRead == 0) {
#if defined(ESP32)
          log_e("Timeout reading chunk size");
#endif
#if defined(ESP8266)
          DEBUG_HTTP_MSG("Timeout reading chunk size");
#endif

          return 0;  // Time out reading chunk header
        }

        if(bytesRead != 1 || chunkHeader[0] != '\r') {
#if defined(ESP32)
          log_e("Chunked transport out of sync");
#endif
#if defined(ESP8266)
          DEBUG_HTTP_MSG("Chunked transport out of sync");
#endif

          return 0;
        }

        bytesRead = _client.readBytesUntil('\n', chunkHeader, sizeof chunkHeader);
        if(bytesRead == 0) {
#if defined(ESP32)
          log_e("Timeout reading chunk size");
#endif
#if defined(ESP8266)
          DEBUG_HTTP_MSG("Timeout reading chunk size");
#endif

          return 0;  // Time out reading chunk header
        }

        if(chunkHeader[bytesRead - 1] != '\r') {
#if defined(ESP32)
          log_e("Invalid chunk header");
#endif
#if defined(ESP8266)
          DEBUG_HTTP_MSG("Invalid chunk header");
#endif

          return 0;  // Invalid chunk header
        }

        _leftToRead = strtol(chunkHeader, NULL, 16);
#if defined(ESP32)
          chunkHeader[bytesRead] = '\0';
          log_v("Chunk header '%s', chunk size %04x (%u)", chunkHeader, _leftToRead, _leftToRead);
#endif

        if(_leftToRead == 0) {
          _state = CLIENT_READY;

          return 0;              
        } else {

          return _client.available();          
        }
      } else {
        _state = CLIENT_READY;

        return 0;
      }
    } else {

      return _client.available();
    }
  } else if(_leftToRead == 0) {
     _state = CLIENT_READY;
  } else {
#if defined(ESP32)
      log_e("Invalid state (%u): available", _state);
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Invalid state (%u): available", _state);
#endif
  }

  return 0;
}


int                 ClientHTTP::read() {
  uint8_t c;
  if(read(&c, 1) != 1) return -1;

  return c;
}

int                 ClientHTTP::read(uint8_t *buf, size_t size) {
  if(available()) {
    size_t bytesRead = _client.read(buf, size);
    _leftToRead -= bytesRead;

    // To prevent _leftToRead being 0 before the end of the stream, so _leftToRead == 0 can be used to check for the end of the stream
    if(_chunked && _leftToRead == 0 && _state != CLIENT_READY) {
      available();
    }

    return bytesRead;
  } else {

    return 0;
  }
}


int                 ClientHTTP::peek() {
  if(available()) {
    return _client.peek();
  } else {
    return -1;
  }
}


void                ClientHTTP::flush() {
  _client.flush();
}


void                ClientHTTP::stop() {
#if defined(ESP32)
  log_v("Stop");
#endif

  _client.stop();
}


uint8_t             ClientHTTP::connected() {
  return _client.connected();
}


                    ClientHTTP::operator bool() {
  return _client;
}


bool                ClientHTTP::writeRequestHeaders(const char * method, const char * uri, size_t contentLength) {
  if(_host == NULL && _ip == INADDR_NONE) {
#if defined(ESP32)
    log_e("Host not set");
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("Host not set");
#endif

    return false;
  }
  if(!_client.connected()) {
    Serial.println("(writeRequestHeaders) Not connected");
#if defined(ESP32)
    log_e("Not connected");
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("Not connected");
#endif

    return false;
  }

  if(_state == CLIENT_RESPONSE_PAYLOAD) {
    if(_leftToRead) {
#if defined(ESP32)
      log_v("Cleaning up %yy bytes", _leftToRead);
#endif
      while(_leftToRead) {
        if(available() == 0) {
          delay(100);
          continue;
        }
        read();
      }
    }
    _state = CLIENT_READY;
  }

  if(_state != CLIENT_READY) {
    Serial.printf("(writeRequestHeaders) Invalid state (%u)\n", _state);
#if defined(ESP32)
    log_e("Invalid state (%u)", _state);
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("Invalid state (%u)", _state);
#endif

    return false;
  }

#if defined(ESP32)
    log_v("%s %s, content length is %u", method, uri, contentLength);
    if(_redirectCount) {
      log_v("%u time(-s) redirected", _redirectCount);
    }
#endif

  if(_redirectCount == 0) {
    strcpy(_originalMethod, method);
  }

//  for(auto responseHeader: responseHeaders) {
//    free(responseHeader.second); // !!!!!! NOT SURE IF THESE POINTERS MUST BE FREE-ED
//    responseHeader.second = NULL;
//Serial.printf("Freed response header '%s', value is '%s'\n", responseHeader.first, responseHeader.second != NULL ? responseHeader.second : "null");
//  }

  _leftToWrite = 0;
  _firstChunkToWrite = true;

  _client.printf("%s %s HTTP/1.1\r\n", method, uri);
  if(_host) {
    _client.printf("Host: %s\r\n", _host);
  } else {
    _client.printf("Host: %s\r\n", _ip.toString().c_str());    
  }

  for(auto requestHeader: requestHeaders) { // std::pair<char *, char *>
//    _client.printf("%s: %s\r\n", requestHeader.first, requestHeader.second);
    _client.printf("%s: %s\r\n", requestHeader.first.c_str(), requestHeader.second.c_str());
#if defined(ESP32)
//    log_v("Request header '%s: %s'", requestHeader.first, requestHeader.second);
    log_v("Request header '%s: %s'", requestHeader.first.c_str(), requestHeader.second.c_str());
#endif
  }

  if(strcmp(method, "POST") == 0) {
    _leftToWrite = contentLength;
    _client.printf("Content-Length: %u\r\n", contentLength);
#if defined(ESP32)
    log_v("Request header 'Content-Length: %u'", contentLength);
#endif
  }

  if(requestHeaders.find("User-Agent") == requestHeaders.end()) {
    _client.print("User-Agent: ESPClientHTTP\r\n");
#if defined(ESP32)
    log_v("Request header 'User-Agent: ESPClientHTTP'");
#endif
  }

  if(httpVersion == HTTP_10 && requestHeaders.find("Connection") == requestHeaders.end()) {
    _client.print("Connection: keep-alive\r\n");
#if defined(ESP32)
    log_v("Request header 'Connection: keep-alive'");
#endif
  }
  
  _client.printf("\r\n");

  _client.flush();

  return true;
}


bool                ClientHTTP::GET(const char * uri) {
  bool success = writeRequestHeaders("GET", uri);
  _state = CLIENT_RESPONSE_HEADER;

  return success;
}


bool                ClientHTTP::HEAD(const char * uri) {
  bool success = writeRequestHeaders("HEAD", uri);
  _state = CLIENT_RESPONSE_HEADER;

  return success;
}


bool                ClientHTTP::POST(const char * uri, size_t contentLength) {
  bool success = writeRequestHeaders("POST", uri, contentLength);
  _state = CLIENT_REQUEST_HEADER_SENT;

  return success;
}


bool                ClientHTTP::PUT(const char * uri, size_t contentLength) {
  bool success = writeRequestHeaders("PUT", uri, contentLength);
  _state = CLIENT_REQUEST_HEADER_SENT;

  return success;
}


bool                ClientHTTP::PATCH(const char * uri, size_t contentLength) {
  bool success = writeRequestHeaders("PATCH", uri, contentLength);
  _state = CLIENT_REQUEST_HEADER_SENT;

  return success;
}


bool                ClientHTTP::DELETE(const char * uri, size_t contentLength) {
  bool success = writeRequestHeaders("DELETE", uri, contentLength);
  _state = CLIENT_REQUEST_HEADER_SENT;

  return success;
}


ClientHTTP::http_code_t ClientHTTP::status() {
  if(_state != CLIENT_RESPONSE_HEADER) {
    return HTTP_ERROR;
  }

  ClientHTTP::http_code_t statusCode = readResponseHeaders();

#if defined(SUPPORT_REDIRECTS)
  // Redirect if status code says so
  while(statusCode == MOVED_PERMANENTLY || statusCode == TEMPORARY_REDIRECT || statusCode == FOUND || statusCode == SEE_OTHER) {
    if(_redirectCount++ == redirectLimit) {
#if defined(ESP32)
      log_e("Redirect limit reached");
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Redirect limit reached");
#endif

      return REDIRECT_LIMIT_REACHED;
    }

    if(leftToRead()) {
      while(leftToRead()) read(); // Replace this line with 'printTo(Serial)' to view the dropped payload
#if defined(ESP32)
      log_v("Dropped sent payload");
#endif
    }

#if defined(ESP32)
    log_v("Redirect to '%s'", _location);
#endif
    if(strchr(_location, ':')) {
#if defined(ESP32)
      log_e("Redirection to (another) host not supported (yet)");
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Redirection to (another) host not supported (yet)");
#endif

      return HTTP_ERROR;
    }

    if(!connected()) {
#if defined(ESP32)
      log_v("(re-)connect");
#endif
      int connectionStatus;
      if(_host != NULL) {
        connectionStatus = connect(_host, _port);
      } else {
        connectionStatus = connect(_ip, _port);        
      }
      if(!connectionStatus) {
        Serial.println("(status) Failed to (re-)connect to host");
#if defined(ESP32)
        log_e("Failed to (re-)connect to host");
#endif
#if defined(ESP8266)
        DEBUG_HTTP_MSG("Failed to (re-)connect to host");
#endif
        _redirectCount = 0;
        strcpy(_originalMethod, "");

        return REDIRECT_CONNECTION_FAILED;
      }
    }
    
    _state = CLIENT_READY;

    bool methodResult;
    if((statusCode == MOVED_PERMANENTLY || statusCode == TEMPORARY_REDIRECT) && (strcmp(_originalMethod, "GET") != 0 || strcmp(_originalMethod, "HEAD") != 0)) {
      if(strcmp(_originalMethod, "POST") == 0) {
        methodResult = POST(_location);      
      } else if(strcmp(_originalMethod, "PUT") == 0) {
        methodResult = PUT(_location);              
      } else if(strcmp(_originalMethod, "PATCH") == 0) {
        methodResult = PATCH(_location);      
      } else if(strcmp(_originalMethod, "DELETE") == 0) {
        methodResult = DELETE(_location);      
      } else {
        Serial.printf("(status) Redirection with method '%s' not supported", _originalMethod);        
#if defined(ESP32)
        log_e("Redirection with method '%s' not supported", _originalMethod);
#endif
#if defined(ESP8266)
        DEBUG_HTTP_MSG("Redirection with method '%s' not supported", _originalMethod);
#endif
        _redirectCount = 0;
        strcpy(_originalMethod, "");

        return REDIRECT_UNKOWN_METHOD;
      }
    } else {
      methodResult = GET(_location);
    }
    
    if(!methodResult) {
#if defined(ESP32)
      log_e("Failed to redirect to '%s'", _location);
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Failed to redirect to '%s'", _location);
#endif
      _redirectCount = 0;
      strcpy(_originalMethod, "");

      return REDIRECT_FAILURE;
    }

    statusCode = readResponseHeaders();
  }

  _redirectCount = 0;
  strcpy(_originalMethod, "");
#endif

  return statusCode;
}


ClientHTTP::http_code_t ClientHTTP::statusAsync() {
  if(_state != CLIENT_RESPONSE_HEADER) {
    return HTTP_ERROR;
  }

  if(!available()) {
    return ASYNC_RESPONSE_HEADERS;
  }

  return status();
}


ClientHTTP::http_code_t ClientHTTP::readResponseHeaders() {
  char headerLine[RESPONSE_HEADER_LINE_SIZE];

  size_t bytesRead = _client.readBytesUntil('\n', headerLine, sizeof headerLine - 1);
  headerLine[bytesRead] = '\0';
  char * statusPtr = strchr(headerLine, ' ');
  if(!statusPtr) return HTTP_ERROR;

  int status = atoi(statusPtr);
#if defined(ESP32)
  log_v("Response status line is '%s', status is %u", headerLine, status);
#endif

  _chunked = false;
  _leftToRead = 0;

#if defined(ESP32)
  log_v("Client timeout is %u\n", _client.getTimeout());
#endif

  for(;;) {
    size_t bytesRead = _client.readBytesUntil('\n', headerLine, sizeof headerLine - 1);
    if(bytesRead == 0) {
#if defined(ESP32)
      log_e("Timeout reading response headers");
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Timeout reading response headers");
#endif

      return RESPONSE_HEADER_TIMEOUT;
    }

    if(headerLine[0] == '\r') break;
    
    headerLine[bytesRead - 1] = '\0';
//    Serial.printf("HEADER '%s'\n", headerLine);

    char * valuePtr = strchr(headerLine, ':');
    if(valuePtr) {
      *valuePtr++ = '\0';
      while(*valuePtr && isspace(*valuePtr)) valuePtr++;  // Skip leading space characters
      if(*valuePtr != '\0') {
#if defined(ESP32)
        log_v("Read response header '%s: %s'", headerLine, valuePtr);
#endif

        // Set response header
        auto search = responseHeaders.find(headerLine);
        if(search != responseHeaders.end()) {
#if defined(ESP32)
          log_v("Set response header '%s: %s'", search->first, valuePtr);
          search->second = valuePtr;
#endif
//        free(responseHeader.second); // !!!!!! NOT SURE IF THESE POINTERS MUST BE FREE-ED
//        responseHeader.second = strdup(valuePtr);
        }
      }
    }

    if(!_chunked && strcasecmp(headerLine, "Transfer-Encoding") == 0) {
      for(size_t index = sizeof "Transfer-Encoding"; headerLine[index] != '\0'; index++) {
        headerLine[index] = tolower(headerLine[index]); // Transfer-Encoding values should be compared case insensitively
      }
      _chunked = (strstr(headerLine + sizeof "Transfer-Encoding", "chunked") != NULL);
#if defined(ESP32)
      log_v("Found Transfer-Encoding response header, chunked is %s", _chunked ? "true": "false");
#endif
    }

    if(!_chunked && _leftToRead == 0 && strcasecmp(headerLine, "content-length") == 0) {
      _leftToRead = strtol(valuePtr, NULL, 10);
#if defined(ESP32)
      log_v("Found 'Content-Length: %u' response header", _leftToRead);
#endif
    }

    if(strcasecmp(headerLine, "location") == 0) {
#if defined(ESP32)
      log_v("Found 'Location: %s' response header", valuePtr);
#endif
      free(_location);
      _location = strdup(valuePtr);
      if(_location == NULL) {
        Serial.println("(readResponseHeaders) Out of memory");
#if defined(ESP32)
        log_e("Out of memory");
#endif
#if defined(ESP8266)
        DEBUG_HTTP_MSG("Out of memory");
#endif

        return OUT_OF_MEMORY;
      }
    }
  }

  if(_chunked) {
    char chunkHeader[7];
    size_t bytesRead = _client.readBytesUntil('\n', chunkHeader, sizeof chunkHeader);
    if(bytesRead == 0) {
#if defined(ESP32)
      log_e("Timeout reading chunk header");
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Timeout reading chunk header");
#endif

      return CHUNK_HEADER_TIMEOUT;  // Time out reading chunk header
    }

    if(chunkHeader[bytesRead - 1] != '\r') {
#if defined(ESP32)
      log_e("Invalid chunk header");
#endif
#if defined(ESP8266)
      DEBUG_HTTP_MSG("Invalid chunk header");
#endif

      return CHUNK_HEADER_INVALID;  // Invalid chunk header
    }

    _leftToRead = strtol(chunkHeader, NULL, 16); 
#if defined(ESP32)
    log_v("Chunk size %04x (%u)", _leftToRead, _leftToRead);
#endif
  }

  _state = CLIENT_RESPONSE_PAYLOAD;

  return (http_code_t)status;
}


/*
const char *        ClientHTTP::errorToString(http_code_t error) {
  switch(error) {
    case ClientHTTPOUT_OF_MEMORY:
      return "Out of memory";
    break;
    default:
      return "No error string available";
  }
}


ClientHTTPhttp_code_t ClientHTTP::returnError(http_code_t error) {
  if(error < 0) {
#if defined(ESP32)
    log_e("Error %d: %s", error, errorToString(error));
#endif
#if defined(ESP8266)
    DEBUG_HTTP_MSG("(ClientHTTPreturnError) Error %d: %s\n", error, errorToString(error));
#endif

  }

  return error;
}
*/


size_t              ClientHTTP::leftToRead() {
  return _leftToRead;
}


size_t              ClientHTTP::leftToWrite() {
  return _leftToWrite;
}


void                ClientHTTP::printTo(Print &printer) {
  while(leftToRead()) {
    uint8_t printBuffer[PRINT_BUFFER_SIZE];

    size_t bytesToRead = available();
    if(bytesToRead > PRINT_BUFFER_SIZE) bytesToRead = PRINT_BUFFER_SIZE;

    size_t bytesRead = read(printBuffer, bytesToRead);
    printer.write(printBuffer, bytesRead);
  }
}
