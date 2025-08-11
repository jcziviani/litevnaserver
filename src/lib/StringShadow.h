// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#include <iostream>
#include <string>

namespace makeland {
    using namespace std;

    // This class does not use std::string_view concepts. They are not interchangeable.
    // Just as a real shadow requires an object to exist, StringShadow requires a fixed sequence of characters in memory during its lifetime.
    class StringShadow {
        friend ostream& operator<<(ostream& os, const StringShadow& s);

    public:
        StringShadow() : _dataSource(""), _size(0) {}
        StringShadow(const char* data, size_t pos, size_t size) : _dataSource(data + pos), _size(size) {}
        StringShadow(const StringShadow& other, size_t pos, size_t size) : _dataSource(other._dataSource + pos), _size(size) {}
        StringShadow(const StringShadow&) = default;
        StringShadow(StringShadow&&) = default;

        StringShadow& operator=(const StringShadow&) = default;
        StringShadow& operator=(StringShadow&&) = default;
        ~StringShadow() = default;

        size_t find(char c, size_t pos = 0) const {
            if (pos > _size) {
                return string::npos;
            }
            void* ret = memchr((char*)_dataSource + pos, c, _size - pos);

            if (!ret) {
                return string::npos;
            }
            return (size_t)((char*)(ret)-_dataSource);
        }

        bool equals(const char* str) const {
            const char* pos = _dataSource;
            const char* end = _dataSource + _size;

            while (pos < end && (*pos != '\0')) {
                if (*pos++ != *str++) {
                    return false;
                }
            }
            if (*str != '\0') {
                return false;
            }
            return true;
        }

        bool equals(const string& str) const {
            return equals(str.data(), str.size());
        }

        bool equals(const char* str, size_t size) const {
            if (size != _size) {
                return false;
            }
            return strncmp(_dataSource, str, size) == 0;
        }

        StringShadow substr(size_t pos) const {
            if (pos > _size) {
                return StringShadow();
            }
            return StringShadow(_dataSource, pos, _size - pos);
        }

        StringShadow substr(size_t pos, size_t len) const {
            if ((pos + len) > _size) {
                return StringShadow();
            }
            return StringShadow(_dataSource, pos, len);
        }

        void clear() {
            _dataSource = "";
            _size = 0;
        }

        const char* dataSource() const {
            return _dataSource;
        }

        size_t size() const {
            return _size;
        }

        string toString() const {
            return string(_dataSource, _size);
        }

        bool operator==(const string& str) const {
            return equals(str);
        }

        bool operator==(const char* str) const {
            return equals(str);
        }

        bool operator==(const StringShadow& other) const {
            return equals(other._dataSource, other._size);
        }

        bool operator!=(const char* str) const {
            return !equals(str);
        }

        bool operator!=(const string& str) const {
            return !equals(str);
        }

        bool operator!=(const StringShadow& other) const {
            return !equals(other._dataSource, other._size);
        }

        char operator [](size_t idx) const {
            return _dataSource[idx];
        }

        StringShadow& operator=(const char* _data) {
            _dataSource = _data;
            _size = strlen(_data);

            return *this;
        }

    private:
        const char* _dataSource;
        size_t _size;
    };

    ostream& operator<<(ostream& os, const StringShadow& s) {
        os << s.toString();
        return os;
    }

    bool operator==(const char* lhs, const StringShadow& rhs)
    {
        return rhs == lhs;
    }

    bool operator==(const string& lhs, const StringShadow& rhs)
    {
        return rhs == lhs;
    }
}