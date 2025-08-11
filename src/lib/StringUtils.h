// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 Julio Cesar Ziviani Alvarez

#pragma once

#ifdef _WIN32
#pragma warning(disable : 4996)
#pragma warning(disable : 4840)
#endif

#include <cstring>
#include <codecvt>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "StringShadow.h"

namespace makeland {
    using namespace std;

    // su = StringUtils
    namespace su {
        StringShadow trim(const StringShadow& s, bool includeBelowSpaceChars = true);
        string trim(const string& s, bool includeBelowSpaceChars = true);

        wstring_convert<codecvt_utf8<wchar_t>, wchar_t> strConverter;

        string toString(const wstring& wstr) {
            return strConverter.to_bytes(wstr);
        }

        wstring toWString(const string& str) {
            return strConverter.from_bytes(str);
        }

        template<typename T>
        T atou(const char* str, size_t size, bool* error = nullptr) {
            T res = 0;

            if (size == 0 || size > 20) {
                if (error) {
                    *error = true;
                }
                return 0;
            }
            for (size_t i = 0; i < size; i++) {
                if (str[i] < '0' || str[i] > '9') {
                    if (error) {
                        *error = true;
                    }
                    return 0;
                }
                res = (T)(res * 10 + str[i] - '0');
            }
            if (error) {
                *error = false;
            }
            return res;
        }

        double atod(const char* str, size_t size, bool* error = nullptr) {
            double res = 0.0;
            double multiplier = 1;
            bool addDecimals = false;
            size_t i = 0;

            if (size == 0 || size > 20) {
                if (error) {
                    *error = true;
                }
                return 0.0;
            }
            if (str[0] == '-') {
                multiplier = -1;
                i++;
            }
            if (str[0] == '+') {
                i++;
            }
            while (i < size) {
                if (str[i] == '.') {
                    addDecimals = true;
                }
                else if (str[i] >= '0' && str[i] <= '9') {
                    res = res * 10.0 + (double)(str[i] - '0');

                    if (addDecimals) {
                        multiplier *= 0.1;
                    }
                }
                else {
                    if (error) {
                        *error = true;
                    }
                    return 0.0;
                }
                i++;
            }
            if (error) {
                *error = false;
            }
            return res * multiplier;
        }

        template<typename T>
        string concat(vector<T> values, string delimiter) {
            string ret;

            for (size_t n = 0; n < values.size(); n++) {
                if (n > 0) {
                    ret += delimiter;
                }
                ret += values[n];
            }
            return ret;
        }

        bool endsWith(string const& s, string const& ending) {
            if (s.size() >= ending.size()) {
                return s.compare(s.size() - ending.size(), ending.size(), ending) == 0;
            }
            return false;
        }

        string format(const char* sFormat) {
            return sFormat;
        }

        bool isDigit(char c)
        {
            return (c >= '0' && c <= '9');
        }

        bool isAlpha(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        bool isAlnum(char c) {
            return isDigit(c) || isAlpha(c);
        }

        bool isIdentifier(StringShadow text) {
            size_t pos = 0;

            while (pos < text.size()) {
                char c = text[pos++];

                if (!isAlnum(c) && c != '.' && c != '_' && c != '-') {
                    return false;
                }
            }
            return true;
        }

        bool isIdentifier(const string& text) {
            return isIdentifier(StringShadow(text.data(), 0, text.size()));
        }

        template<typename T, typename... Args>
        string format(const char* sFormat, T value, Args... args) {
            stringstream s;

            while (*sFormat != '\0') {
                if (*sFormat == '{' && *(sFormat + 1) == '}') {
                    string remaining = su::format(sFormat + 2, args...);
                    s << value << remaining;
                    return s.str();
                }
                s << *sFormat;
                sFormat++;
            }
            return s.str();
        }

        template<typename T>
        size_t itoa(T n, char* s, size_t size) {
            static const char* validChars = "0123456789";
            lldiv_t qr;
            qr.quot = n;

            bool negative = false;

            if (n < 0) {
                n = -n;
                negative = true;
            }
            if (!size) {
                do {
                    qr = lldiv(qr.quot, 10);
                    size++;
                } while (qr.quot);
                qr.quot = n;
            }
            char* p = s + size;
            size_t ret = size;

            if (negative) {
                ret++;
                p++;
                *s = '-';
            }
            do {
                qr = lldiv(qr.quot, 10);
                *(--p) = validChars[abs(qr.rem)];
            } while (--size);

            return ret;
        }

        template<typename T>
        size_t utoa(T n, char* s, size_t size) {
            static const char* validChars = "0123456789";
            T quot = n;

            if (!size) {
                do {
                    quot = quot / 10;
                    size++;
                } while (quot);

                quot = n;
            }
            char* p = s + size;
            size_t ret = size;

            do {
                *(--p) = validChars[quot % 10];
                quot = quot / 10;
            } while (--size);

            return ret;
        }

        string escape(const string& text) {
            static const char* digits = "0123456789ABCDEF";
            string ret;

            for (size_t pos = 0; pos < text.size(); pos++) {
                uint8_t c = (uint8_t)text[pos];

                if (c == '\n') {
                    ret += "\\n";
                }
                else if (c < 32 || c > 126) {
                    ret += "\\x";
                    ret += digits[c >> 4];
                    ret += digits[c & 0xF];
                }
                else {
                    ret += (char)c;
                }
            }
            return ret;
        }

        string escapeJSON(const string& text) {
            static const char* digits = "0123456789ABCDEF";
            string ret;

            for (size_t pos = 0; pos < text.size(); pos++) {
                uint8_t c = (uint8_t)text[pos];

                if (c == '\"' || c == '\'' || c == '{' || c == '}' || c == '[' || c == ']' || c < 32 || c > 126) {
                    ret += "\\u00";
                    ret += digits[c >> 4];
                    ret += digits[c & 0xF];
                }
                else {
                    ret += (char)c;
                }
            }
            return ret;
        }

        bool validateEmail(const string& email) {
            size_t pos = 0;
            bool special = false;

            while (pos < email.size()) {
                char c = email[pos++];

                if (isalnum(c)) {
                    special = false;
                    continue;
                }
                if (c == '_' || c == '.' || c == '-') {
                    if (special) {
                        return false;
                    }
                    special = true;
                    continue;
                }
                if (c == '@') {
                    break;
                }
                return false;
            }
            if (pos >= email.size()) {
                return false;
            }
            bool lastDot = false;
            int  dotCount = 0;

            while (pos < email.size()) {
                char c = email[pos++];

                if (isalnum(c) || c == '-') {
                    lastDot = false;
                    continue;
                }
                if (c == '.') {
                    if (lastDot) {
                        return false;
                    }
                    lastDot = true;
                    dotCount++;
                }
            }
            if (dotCount == 0) {
                return false;
            }
            return true;
        }

        string replaceAll(const string& text, const string& from, const string& to) {
            if (from.size() == 0) {
                return text;
            }
            string s = text;
            size_t startPos = 0;

            while ((startPos = s.find(from, startPos)) != string::npos) {
                s.replace(startPos, from.size(), to);
                startPos += to.size();
            }
            return s;
        }

        vector<string> slice(const string& s, const string& delimiter, bool _trim) {
            size_t start = 0;
            size_t end = 0;
            string token;
            string temp = s;
            vector<string> ret;

            if (_trim) {
                temp = trim(s);
            }
            if (temp.size() == 0) {
                return ret;
            }
            if (delimiter.size() == 0) {
                return ret;
            }
            while ((end = temp.find(delimiter, start + delimiter.size())) != string::npos) {
                token = temp.substr(start, end - start);
                start = end;
                ret.push_back(_trim ? trim(token) : token);
            }
            ret.push_back(_trim ? trim(temp.substr(start)) : temp.substr(start));
            return ret;
        }

        vector<string> split(const StringShadow& s, char delimiter, bool _trim) {
            size_t start = 0;
            size_t end = 0;
            StringShadow token;
            vector<string> ret;

            if (_trim && s.size() == 0) {
                return ret;
            }
            while ((end = s.find(delimiter, start)) != string::npos) {
                token = s.substr(start, end - start);
                start = end + 1;
                ret.push_back(_trim ? trim(token).toString() : token.toString());
            }
            ret.push_back(_trim ? trim(s.substr(start)).toString() : s.substr(start).toString());
            return ret;
        }

        vector<string> split(const string& s, char delimiter, bool _trim) {
            size_t start = 0;
            size_t end = 0;
            string token;
            vector<string> ret;

            if (_trim && s.size() == 0) {
                return ret;
            }
            while ((end = s.find(delimiter, start)) != string::npos) {
                token = s.substr(start, end - start);
                start = end + 1;
                ret.push_back(_trim ? trim(token) : token);
            }
            ret.push_back(_trim ? trim(s.substr(start)) : s.substr(start));
            return ret;
        }

        bool startsWith(const StringShadow& s, const string& starting) {
            if (s.size() >= starting.size()) {
                return s.substr(0, starting.size()).equals(starting);
            }
            return false;
        }

        bool startsWith(const string& s, const string& starting) {
            if (s.size() >= starting.size()) {
                return s.compare(0, starting.size(), starting) == 0;
            }
            return false;
        }

        /*
        https://github.com/superwills/NibbleAndAHalf
        base64.h -- Fast base64 encoding and decoding.
        version 1.0.0, April 17, 2013 143a
        Copyright (C) 2013 William Sherif
        This software is provided 'as-is', without any express or implied
        warranty.  In no event will the authors be held liable for any damages
        arising from the use of this software.
        Permission is granted to anyone to use this software for any purpose,
        including commercial applications, and to alter it and redistribute it
        freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software
        in a product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not be
        misrepresented as being the original software.
        3. This notice may not be removed or altered from any source distribution.
        William Sherif
        will.sherif@gmail.com
        YWxsIHlvdXIgYmFzZSBhcmUgYmVsb25nIHRvIHVz
        */
        string encodeBase64url(const StringShadow& data) {
            const static char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            const unsigned char* bin = (const unsigned char*)data.dataSource();
            size_t len = data.size();

            size_t rc = 0; // result counter
            size_t byteNo; // I need this after the loop
            size_t modulusLen = len % 3;
            size_t pad = ((modulusLen & 1) << 1) + ((modulusLen & 2) >> 1); // 2 gives 1 and 1 gives 2, but 0 gives 0.

            size_t flen = 4 * (len + pad) / 3;
            std::string resStr;
            resStr.resize(flen);

            char* res = &resStr[0];

            if (!res) {
                return "";
            }
            for (byteNo = 0; byteNo <= len - 3; byteNo += 3) {
                unsigned char BYTE0 = bin[byteNo];
                unsigned char BYTE1 = bin[byteNo + 1];
                unsigned char BYTE2 = bin[byteNo + 2];
                res[rc++] = b64[BYTE0 >> 2];
                res[rc++] = b64[((0x3 & BYTE0) << 4) + (BYTE1 >> 4)];
                res[rc++] = b64[((0x0f & BYTE1) << 2) + (BYTE2 >> 6)];
                res[rc++] = b64[0x3f & BYTE2];
            }
            if (pad == 2) {
                res[rc++] = b64[bin[byteNo] >> 2];
                res[rc++] = b64[(0x3 & bin[byteNo]) << 4];
                res[rc++] = '=';
                res[rc++] = '=';
            }
            else if (pad == 1) {
                res[rc++] = b64[bin[byteNo] >> 2];
                res[rc++] = b64[((0x3 & bin[byteNo]) << 4) + (bin[byteNo + 1] >> 4)];
                res[rc++] = b64[(0x0f & bin[byteNo + 1]) << 2];
                res[rc++] = '=';
            }
            return resStr;
        }

        // https://github.com/mllg/base64url/blob/master/src/base64.c

        string decodeBase64url(const StringShadow& encoded) {
            static const unsigned char ascii[] = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 63,
                52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
                0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 63,
                0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };
            if (encoded.size() == 0) {
                return "";
            }
            size_t allocationSize = (((encoded.size() + 3) / 4) * 3) + 1;
            string buffer;
            buffer.resize(allocationSize);
            const unsigned char* pos = (const unsigned char*)encoded.dataSource();
            const unsigned char* end = pos + encoded.size();
            unsigned char* out = (unsigned char*)&buffer[0];

            while (pos < (end - 4)) {
                out[0] = (unsigned char)(ascii[pos[0]] << 2 | ascii[pos[1]] >> 4);
                out[1] = (unsigned char)(ascii[pos[1]] << 4 | ascii[pos[2]] >> 2);
                out[2] = (unsigned char)(ascii[pos[2]] << 6 | ascii[pos[3]]);
                pos += 4;
                out += 3;
            }
            if ((end - pos) > 1) {
                *(out++) = (unsigned char)(ascii[pos[0]] << 2 | ascii[pos[1]] >> 4);
            }
            if ((end - pos) > 2) {
                *(out++) = (unsigned char)(ascii[pos[1]] << 4 | ascii[pos[2]] >> 2);
            }
            if ((end - pos) > 3) {
                *(out++) = (unsigned char)(ascii[pos[2]] << 6 | ascii[pos[3]]);
            }
            buffer.resize((size_t)(out - (unsigned char*)&buffer[0]));
            return buffer;
        }

        string toLower(const string& text) {
            string ret;

            for (size_t n = 0; n < text.size(); n++) {
                ret += (char)tolower(text[n]);
            }
            return ret;
        }

        string toUpper(const string& text) {
            string ret;

            for (size_t n = 0; n < text.size(); n++) {
                ret += (char)toupper(text[n]);
            }
            return ret;
        }

        StringShadow trim(const StringShadow& s, bool includeBelowSpaceChars) {
            size_t start = 0;
            size_t end = 0;

            if (s.size() == 0) {
                return StringShadow();
            }
            if (includeBelowSpaceChars) {
                while (start < s.size() && s[start] <= ' ') {
                    start++;
                }
                end = s.size() - 1;

                while (end > 0 && s[end] <= ' ') {
                    end--;
                }
            }
            else {
                while (start < s.size() && s[start] == ' ') {
                    start++;
                }
                end = s.size() - 1;

                while (end > 0 && s[end] == ' ') {
                    end--;
                }
            }
            if (end >= start) {
                return s.substr(start, end - start + 1);
            }
            return StringShadow();
        }

        string trim(const string& s, bool includeBelowSpaceChars) {
            size_t start = 0;
            size_t end = 0;

            if (s.size() == 0) {
                return "";
            }
            if (includeBelowSpaceChars) {
                while (start < s.size() && s[start] <= ' ') {
                    start++;
                }
                end = s.size() - 1;

                while (end > 0 && s[end] <= ' ') {
                    end--;
                }
            }
            else {
                while (start < s.size() && s[start] == ' ') {
                    start++;
                }
                end = s.size() - 1;

                while (end > 0 && s[end] == ' ') {
                    end--;
                }
            }
            if (end >= start) {
                return string(s.data() + start, end - start + 1);
            }
            return "";
        }

        template<typename T>
        size_t utoa(T n, char* s, size_t size, uint8_t base) {
            static const char* validChars = "0123456789ABCDEF";
            lldiv_t qr;
            qr.quot = (long long)n;

            if (base > 16) {
                base = 16;
            }
            if (!size) {
                do {
                    qr = lldiv(qr.quot, base);
                    size++;
                } while (qr.quot);
                qr.quot = (long long)n;
            }
            if (!s) {
                return size;
            }
            char* p = s + size;
            size_t ret = size;

            do {
                qr = lldiv(qr.quot, base);
                *(--p) = validChars[abs(qr.rem)];
            } while (--size);

            return ret;
        }

        template<typename T>
        string toHex(T value, size_t size = 0) {
            if (size > 50) {
                return "*ERROR* size to large";
            }
            char buffer[50];
            size_t resultSize = utoa(value, buffer, size, 16);
            return string(buffer, resultSize);
        }
    }
}