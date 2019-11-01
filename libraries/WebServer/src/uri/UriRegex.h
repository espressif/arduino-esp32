#ifndef URI_REGEX_H
#define URI_REGEX_H

#include "Uri.h"
#include <regex>

class UriRegex : public Uri {

    public:
        explicit UriRegex(const char *uri) : Uri(uri) {};
        explicit UriRegex(const String &uri) : Uri(uri) {};

        Uri* clone() const override final {
            return new UriRegex(_uri);
        };

        void initPathArgs(std::vector<String> &pathArgs) override final {
            std::regex rgx((_uri + "|").c_str());
            std::smatch matches;
            std::string s{""};
            std::regex_search(s, matches, rgx);
            pathArgs.resize(matches.size() - 1);
        }

        bool canHandle(const String &requestUri, std::vector<String> &pathArgs) override final {
            if (Uri::canHandle(requestUri, pathArgs))
                return true;

            unsigned int pathArgIndex = 0;
            std::regex rgx(_uri.c_str());
            std::smatch matches;
            std::string s(requestUri.c_str());
            if (std::regex_search(s, matches, rgx)) {
                for (size_t i = 1; i < matches.size(); ++i) {  // skip first
                    pathArgs[pathArgIndex] = String(matches[i].str().c_str());
                    pathArgIndex++;
                }
                return true;
            }
            return false;
        }
};

#endif
