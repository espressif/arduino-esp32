/*
 WString.h - String library for Wiring & Arduino
 ...mostly rewritten by Paul Stoffregen...
 Copyright (c) 2009-10 Hernando Barragan.  All right reserved.
 Copyright 2011, Paul Stoffregen, paul@pjrc.com

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef String_class_h
#define String_class_h
#ifdef __cplusplus

#include <pgmspace.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>


// A pure abstract class forward used as a means to proide a unique pointer type
// but really is never defined.
class __FlashStringHelper;
#define FPSTR(str_pointer) (reinterpret_cast<const __FlashStringHelper *>(str_pointer))
#define F(string_literal) (FPSTR(PSTR(string_literal)))

// An inherited class for holding the result of a concatenation.  These
// result objects are assumed to be writable by subsequent concatenations.
class StringSumHelper;

// The string class
class String {
        // use a function pointer to allow for "if (s)" without the
        // complications of an operator bool(). for more information, see:
        // http://www.artima.com/cppsource/safebool.html
        typedef void (String::*StringIfHelperType)() const;
        void StringIfHelper() const {
        }

    public:
        // constructors
        // creates a copy of the initial value.
        // if the initial value is null or invalid, or if memory allocation
        // fails, the string will be marked as invalid (i.e. "if (s)" will
        // be false).
        String(const char *cstr = "");
        String(const char *cstr, unsigned int length);
#ifdef __GXX_EXPERIMENTAL_CXX0X__
        String(const uint8_t *cstr, unsigned int length) : String(reinterpret_cast<const char*>(cstr), length) {}
#endif
        String(const String &str);
        String(const __FlashStringHelper *str) : String(reinterpret_cast<const char*>(str)) {}
#ifdef __GXX_EXPERIMENTAL_CXX0X__
        String(String &&rval);
        String(StringSumHelper &&rval);
#endif
        explicit String(char c);
        explicit String(unsigned char, unsigned char base = 10);
        explicit String(int, unsigned char base = 10);
        explicit String(unsigned int, unsigned char base = 10);
        explicit String(long, unsigned char base = 10);
        explicit String(unsigned long, unsigned char base = 10);
        explicit String(float, unsigned int decimalPlaces = 2);
        explicit String(double, unsigned int decimalPlaces = 2);
        explicit String(long long, unsigned char base = 10);
        explicit String(unsigned long long, unsigned char base = 10);
        ~String(void);

        // memory management
        // return true on success, false on failure (in which case, the string
        // is left unchanged).  reserve(0), if successful, will validate an
        // invalid string (i.e., "if (s)" will be true afterwards)
        bool reserve(unsigned int size);
        inline unsigned int length(void) const {
            if(buffer()) {
                return len();
            } else {
                return 0;
            }
        }
        inline void clear(void) {
            setLen(0);
        }
        inline bool isEmpty(void) const {
            return length() == 0;
        }

        // creates a copy of the assigned value.  if the value is null or
        // invalid, or if the memory allocation fails, the string will be
        // marked as invalid ("if (s)" will be false).
        String & operator =(const String &rhs);
        String & operator =(const char *cstr);
        String & operator = (const __FlashStringHelper *str) {return *this = reinterpret_cast<const char*>(str);}
#ifdef __GXX_EXPERIMENTAL_CXX0X__
        String & operator =(String &&rval);
        String & operator =(StringSumHelper &&rval);
#endif

        // concatenate (works w/ built-in types, same as assignment)

        // returns true on success, false on failure (in which case, the string
        // is left unchanged).  if the argument is null or invalid, the
        // concatenation is considered unsuccessful.
        bool concat(const String &str);
        bool concat(const char *cstr);
        bool concat(const char *cstr, unsigned int length);
        bool concat(const uint8_t *cstr, unsigned int length) {return concat(reinterpret_cast<const char*>(cstr), length);}
        bool concat(char c);
        bool concat(unsigned char c);
        bool concat(int num);
        bool concat(unsigned int num);
        bool concat(long num);
        bool concat(unsigned long num);
        bool concat(float num);
        bool concat(double num);
        bool concat(long long num);
        bool concat(unsigned long long num);
        bool concat(const __FlashStringHelper * str) {return concat(reinterpret_cast<const char*>(str));}

        // if there's not enough memory for the concatenated value, the string
        // will be left unchanged (but this isn't signalled in any way)
        String & operator +=(const String &rhs) {
            concat(rhs);
            return (*this);
        }
        String & operator +=(const char *cstr) {
            concat(cstr);
            return (*this);
        }
        String & operator +=(char c) {
            concat(c);
            return (*this);
        }
        String & operator +=(unsigned char num) {
            concat(num);
            return (*this);
        }
        String & operator +=(int num) {
            concat(num);
            return (*this);
        }
        String & operator +=(unsigned int num) {
            concat(num);
            return (*this);
        }
        String & operator +=(long num) {
            concat(num);
            return (*this);
        }
        String & operator +=(unsigned long num) {
            concat(num);
            return (*this);
        }
        String & operator +=(float num) {
            concat(num);
            return (*this);
        }
        String & operator +=(double num) {
            concat(num);
            return (*this);
        }
        String & operator +=(long long num) {
            concat(num);
            return (*this);
        }
        String & operator +=(unsigned long long num) {
            concat(num);
            return (*this);
        }
        String & operator += (const __FlashStringHelper *str) {return *this += reinterpret_cast<const char*>(str);}

        friend StringSumHelper & operator +(const StringSumHelper &lhs, const String &rhs);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, const char *cstr);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, char c);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, unsigned char num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, int num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, unsigned int num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, long num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, unsigned long num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, float num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, double num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, long long num);
        friend StringSumHelper & operator +(const StringSumHelper &lhs, unsigned long long num);

        // comparison (only works w/ Strings and "strings")
        operator StringIfHelperType() const {
            return buffer() ? &String::StringIfHelper : 0;
        }
        int compareTo(const String &s) const;
        bool equals(const String &s) const;
        bool equals(const char *cstr) const;
        bool operator ==(const String &rhs) const {
            return equals(rhs);
        }
        bool operator ==(const char *cstr) const {
            return equals(cstr);
        }
        bool operator !=(const String &rhs) const {
            return !equals(rhs);
        }
        bool operator !=(const char *cstr) const {
            return !equals(cstr);
        }
        bool operator <(const String &rhs) const;
        bool operator >(const String &rhs) const;
        bool operator <=(const String &rhs) const;
        bool operator >=(const String &rhs) const;
        bool equalsIgnoreCase(const String &s) const;
        unsigned char equalsConstantTime(const String &s) const;
        bool startsWith(const String &prefix) const;
        bool startsWith(const char *prefix) const {
            return this->startsWith(String(prefix));
        }
        bool startsWith(const __FlashStringHelper *prefix) const {
            return this->startsWith(reinterpret_cast<const char*>(prefix));
        }
        bool startsWith(const String &prefix, unsigned int offset) const;
        bool endsWith(const String &suffix) const;
        bool endsWith(const char *suffix) const {
            return this->endsWith(String(suffix));
        }
        bool endsWith(const __FlashStringHelper * suffix) const {
            return this->endsWith(reinterpret_cast<const char*>(suffix));
        }

        // character access
        char charAt(unsigned int index) const;
        void setCharAt(unsigned int index, char c);
        char operator [](unsigned int index) const;
        char& operator [](unsigned int index);
        void getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index = 0) const;
        void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const {
            getBytes((unsigned char *) buf, bufsize, index);
        }
        const char* c_str() const { return buffer(); }
        char* begin() { return wbuffer(); }
        char* end() { return wbuffer() + length(); }
        const char* begin() const { return c_str(); }
        const char* end() const { return c_str() + length(); }

        // search
        int indexOf(char ch) const;
        int indexOf(char ch, unsigned int fromIndex) const;
        int indexOf(const String &str) const;
        int indexOf(const String &str, unsigned int fromIndex) const;
        int lastIndexOf(char ch) const;
        int lastIndexOf(char ch, unsigned int fromIndex) const;
        int lastIndexOf(const String &str) const;
        int lastIndexOf(const String &str, unsigned int fromIndex) const;
        String substring(unsigned int beginIndex) const {
            return substring(beginIndex, len());
        }
        String substring(unsigned int beginIndex, unsigned int endIndex) const;

        // modification
        void replace(char find, char replace);
        void replace(const String &find, const String &replace);
        void replace(const char *find, const String &replace) {
            this->replace(String(find), replace);
        }
        void replace(const __FlashStringHelper *find, const String &replace) {
            this->replace(reinterpret_cast<const char*>(find), replace);
        }
        void replace(const char *find, const char *replace) {
            this->replace(String(find), String(replace));
        }
        void replace(const __FlashStringHelper *find, const char *replace) {
            this->replace(reinterpret_cast<const char*>(find), String(replace));
        }
        void replace(const __FlashStringHelper *find, const __FlashStringHelper *replace) {
            this->replace(reinterpret_cast<const char*>(find), reinterpret_cast<const char*>(replace));
        }
        void remove(unsigned int index);
        void remove(unsigned int index, unsigned int count);
        void toLowerCase(void);
        void toUpperCase(void);
        void trim(void);

        // parsing/conversion
        long toInt(void) const;
        float toFloat(void) const;
	double toDouble(void) const;

    protected:
        // Contains the string info when we're not in SSO mode
        struct _ptr { 
            char *   buff;
            uint32_t cap;
            uint32_t len;
        };
        // This allows strings up up to 11 (10 + \0 termination) without any extra space.
        enum { SSOSIZE = sizeof(struct _ptr) + 4 - 1 }; // Characters to allocate space for SSO, must be 12 or more
        struct _sso {
            char     buff[SSOSIZE];
            unsigned char len   : 7; // Ensure only one byte is allocated by GCC for the bitfields
            unsigned char isSSO : 1;
        } __attribute__((packed)); // Ensure that GCC doesn't expand the flag byte to a 32-bit word for alignment issues
#ifdef BOARD_HAS_PSRAM
        enum { CAPACITY_MAX = 3145728 }; 
#else
        enum { CAPACITY_MAX = 65535 }; 
#endif
        union {
            struct _ptr ptr;
            struct _sso sso;
        };
        // Accessor functions
        inline bool isSSO() const { return sso.isSSO; }
        inline unsigned int len() const { return isSSO() ? sso.len : ptr.len; }
        inline unsigned int capacity() const { return isSSO() ? (unsigned int)SSOSIZE - 1 : ptr.cap; } // Size of max string not including terminal NUL
        inline void setSSO(bool set) { sso.isSSO = set; }
        inline void setLen(int len) {
            if (isSSO()) {
                sso.len = len;
                sso.buff[len] = 0;
            } else {
                ptr.len = len;
                if (ptr.buff) {
                    ptr.buff[len] = 0;
                }
            }
        }
        inline void setCapacity(int cap) { if (!isSSO()) ptr.cap = cap; }
        inline void setBuffer(char *buff) { if (!isSSO()) ptr.buff = buff; }
        // Buffer accessor functions
        inline const char *buffer() const { return reinterpret_cast<const char *>(isSSO() ? sso.buff : ptr.buff); }
        inline char *wbuffer() const { return isSSO() ? const_cast<char *>(sso.buff) : ptr.buff; } // Writable version of buffer

    protected:
        void init(void);
        void invalidate(void);
        bool changeBuffer(unsigned int maxStrLen);

        // copy and move
        String & copy(const char *cstr, unsigned int length);
        String & copy(const __FlashStringHelper *pstr, unsigned int length) {
                return copy(reinterpret_cast<const char*>(pstr), length);
        }
#ifdef __GXX_EXPERIMENTAL_CXX0X__
        void move(String &rhs);
#endif
};

class StringSumHelper: public String {
    public:
        StringSumHelper(const String &s) :
                String(s) {
        }
        StringSumHelper(const char *p) :
                String(p) {
        }
        StringSumHelper(char c) :
                String(c) {
        }
        StringSumHelper(unsigned char num) :
                String(num) {
        }
        StringSumHelper(int num) :
                String(num) {
        }
        StringSumHelper(unsigned int num) :
                String(num) {
        }
        StringSumHelper(long num) :
                String(num) {
        }
        StringSumHelper(unsigned long num) :
                String(num) {
        }
        StringSumHelper(float num) :
                String(num) {
        }
        StringSumHelper(double num) :
                String(num) {
        }
        StringSumHelper(long long num) :
                String(num) {
        }
        StringSumHelper(unsigned long long num) :
                String(num) {
        }
};
        
inline StringSumHelper & operator +(const StringSumHelper &lhs, const __FlashStringHelper *rhs) {
        return lhs + reinterpret_cast<const char*>(rhs);
}

extern const String emptyString;

#endif  // __cplusplus
#endif  // String_class_h
