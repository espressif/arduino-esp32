#ifndef URI_BRACES_H
#define URI_BRACES_H

#include "Uri.h"

class UriBraces : public Uri {

    public:
        explicit UriBraces(const char *uri) : Uri(uri) {};
        explicit UriBraces(const String &uri) : Uri(uri) {};

        Uri* clone() const override final {
            return new UriBraces(_uri);
        };

        void initPathArgs(std::vector<String> &pathArgs) override final {
            int numParams = 0, start = 0;
            do {
                start = _uri.indexOf("{}", start);
                if (start > 0) {
                    numParams++;
                    start += 2;
                }
            } while (start > 0);
            pathArgs.resize(numParams);
        }

        bool canHandle(const String &requestUri, std::vector<String> &pathArgs) override final {
            if (Uri::canHandle(requestUri, pathArgs))
                return true;

            size_t uriLength = _uri.length();
            unsigned int pathArgIndex = 0;
            unsigned int requestUriIndex = 0;
            for (unsigned int i = 0; i < uriLength; i++, requestUriIndex++) {
                char uriChar = _uri[i];
                char requestUriChar = requestUri[requestUriIndex];

                if (uriChar == requestUriChar)
                    continue;
                if (uriChar != '{')
                    return false;

                i += 2; // index of char after '}'
                if (i >= uriLength) {
                    // there is no char after '}'
                    pathArgs[pathArgIndex] = requestUri.substring(requestUriIndex);
                    return pathArgs[pathArgIndex].indexOf("/") == -1; // path argument may not contain a '/'
                }
                else
                {
                    char charEnd = _uri[i];
                    int uriIndex = requestUri.indexOf(charEnd, requestUriIndex);
                    if (uriIndex < 0)
                        return false;
                    pathArgs[pathArgIndex] = requestUri.substring(requestUriIndex, uriIndex);
                    requestUriIndex = (unsigned int) uriIndex;
                }
                pathArgIndex++;
            }

            return requestUriIndex >= requestUri.length();
        }
};

#endif
