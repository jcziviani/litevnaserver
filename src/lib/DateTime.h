// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#include <chrono>

#include "StringUtils.h"

namespace makeland {
    struct DateTime {
        uint16_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint8_t second = 0;
        uint32_t microsecond = 0;
        uint64_t timestamp = 0;  // Unix timestamp in microseconds

        DateTime() = default;

        explicit DateTime(uint64_t timestampMicroseconds) {
            set(timestampMicroseconds);
        }

        void clear() {
            year = 0;
            month = 0;
            day = 0;
            hour = 0;
            minute = 0;
            second = 0;
            microsecond = 0;
            timestamp = 0;
        }

        bool isValid() const {
            if (year < 1600 || year > 2500) {
                return false;
            }
            if (month < 1 || month > 12) {
                return false;
            }
            int daysInMonth = (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;

            if (day < 1 || day > daysInMonth)
                return false;

            if (month == 2) {
                bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);

                if (leap) {
                    if (day > 29) {
                        return false;
                    }
                }
                else if (day > 28) {
                    return false;
                }
            }
            if (hour > 23) {
                return false;
            }
            if (minute > 59) {
                return false;
            }
            if (second > 59) {
                return false;
            }
            if (microsecond > 999'999UL) {
                return false;
            }
            return true;
        }

        void addSeconds(int seconds) {
            set(timestamp + (uint64_t)seconds * 1'000'000UL);
        }

        void addMinutes(int minutes) {
            set(timestamp + (uint64_t)minutes * 60UL * 1'000'000UL);
        }

        void addHours(int hours) {
            set(timestamp + (uint64_t)hours * 60UL * 60 * 1'000'000UL);
        }

        void addDays(int days) {
            set(timestamp + (uint64_t)days * 24UL * 60 * 60 * 1'000'000UL);
        }

        bool set(int _year, int _month, int _day, int _hour, int _minute, int _second, int _microsecond) {
            DateTime temp;
            temp.year = (uint16_t)_year;
            temp.month = (uint8_t)_month;
            temp.day = (uint8_t)_day;
            temp.hour = (uint8_t)_hour;
            temp.minute = (uint8_t)_minute;
            temp.second = (uint8_t)_second;
            temp.microsecond = (uint32_t)_microsecond;

            if (!temp.isValid()) {
                return false;
            }
            *this = temp;
            timestamp = getTimestamp(temp);

            return true;
        }

        /* text must be in format "yyyy/MM/dd hh:mm:ss.nnnnnn" where:
           yyyy      full year
           MM        month from 1 to 12
           dd        day of month
           hh        hour from 0 to 23
           mm        minutes from 0 to 59
           ss        seconds from 0 to 59
           nnnnnn    microseconds
        */
        bool parse(const string& text) {
            if (text.size() != 26) {
                return false;
            }
            const char* p = text.data();
            DateTime temp;

            temp.year = su::atou<uint16_t>(p, 4);
            p += 4;

            if (*p++ != '/') {
                return false;
            }
            temp.month = su::atou<uint8_t>(p, 2);
            p += 2;

            if (*p++ != '/') {
                return false;
            }
            temp.day = su::atou<uint8_t>(p, 2);
            p += 2;

            if (*p++ != ' ') {
                return false;
            }
            temp.hour = su::atou<uint8_t>(p, 2);
            p += 2;

            if (*p++ != ':') {
                return false;
            }
            temp.minute = su::atou<uint8_t>(p, 2);
            p += 2;

            if (*p++ != ':') {
                return false;
            }
            temp.second = su::atou<uint8_t>(p, 2);
            p += 2;

            if (*p++ != '.') {
                return false;
            }
            temp.microsecond = su::atou<uint32_t>(p, 6);

            if (!temp.isValid()) {
                return false;
            }
            *this = temp;
            timestamp = getTimestamp(temp);
            return true;
        }

        string toString(int utcOffsetMinutes) const {
            int64_t offset = (int64_t)utcOffsetMinutes * 60LL * (1'000'000LL);
            DateTime temp(this->timestamp + offset);
            char buffer[27];
            char* pos = buffer;

            su::utoa(temp.year, pos, 4, 10);
            pos += 4;
            *pos++ = '/';

            su::utoa(temp.month, pos, 2, 10);
            pos += 2;
            *pos++ = '/';

            su::utoa(temp.day, pos, 2, 10);
            pos += 2;
            *pos++ = ' ';

            su::utoa(temp.hour, pos, 2, 10);
            pos += 2;
            *pos++ = ':';

            su::utoa(temp.minute, pos, 2, 10);
            pos += 2;
            *pos++ = ':';

            su::utoa(temp.second, pos, 2, 10);
            pos += 2;
            *pos++ = '.';

            su::utoa(temp.microsecond, pos, 6, 10);
            pos += 6;
            *pos++ = '\0';

            return buffer;
        }

        string toTime(int utcOffsetMinutes) const {
            int64_t offset = (int64_t)utcOffsetMinutes * 60LL * (1'000'000LL);
            DateTime temp(this->timestamp + offset);
            char buffer[27]; // TODO contar direito
            char* pos = buffer;

            su::utoa(temp.hour, pos, 2, 10);
            pos += 2;
            *pos++ = ':';

            su::utoa(temp.minute, pos, 2, 10);
            pos += 2;
            *pos++ = ':';

            su::utoa(temp.second, pos, 2, 10);
            pos += 2;
            *pos++ = '.';

            su::utoa(temp.microsecond, pos, 6, 10);
            pos += 6;
            *pos++ = '\0';

            return buffer;
        }

        // UTC time since epoch in seconds
        static uint64_t nowSeconds() {
            return (uint64_t)chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
        }

        static uint64_t nowMilliseconds() {
            return (uint64_t)chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        }

        static uint64_t nowMicroseconds() {
            return (uint64_t)chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();
        }

        static uint64_t nowNanoseconds() {
            return (uint64_t)chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();
        }

    private:
        void set(uint64_t _timestamp) {
            this->timestamp = _timestamp;
            this->microsecond = (uint32_t)(timestamp % (1'000'000UL));

            const uint64_t seconds = (uint64_t)(timestamp / (1'000'000UL));
            int days = (int)(seconds / (24LL * 60 * 60));
            int secondsInDay = (int)(seconds % (24LL * 60 * 60));

            this->hour = (uint8_t)(secondsInDay / 3600);
            this->minute = (uint8_t)((secondsInDay % 3600) / 60);
            this->second = (uint8_t)((secondsInDay % 3600) % 60);

            civilFromDays(days, *this);
        }

        uint64_t getTimestamp(const DateTime& dt) {
            int days = daysFromCivil(dt.year, dt.month, dt.day);
            uint64_t seconds = (uint64_t)((days * 24LL * 60 * 60) + (dt.hour * 60LL * 60) + (dt.minute * 60LL) + dt.second);
            return seconds * 1'000'000UL + dt.microsecond;
        }

        // code from http://howardhinnant.github.io/date_algorithms.html
        void civilFromDays(int days, DateTime& dt) {
            days += 719468;
            const int era = days / 146097;
            const unsigned doe = static_cast<unsigned>(days - era * 146097);             // [0, 146096]
            const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;  // [0, 399]
            const int y = static_cast<int>(yoe) + era * 400;
            const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);  // [0, 365]
            const unsigned mp = (5 * doy + 2) / 153;                       // [0, 11]
            const unsigned d = doy - (153 * mp + 2) / 5 + 1;               // [1, 31]
            const unsigned m = mp < 10 ? mp + 3 : mp - 9;                  // [1, 12]
            dt.year = (uint16_t)(y + (m <= 2));
            dt.month = (uint8_t)m;
            dt.day = (uint8_t)d;
        }

        // code from http://howardhinnant.github.io/date_algorithms.html
        int daysFromCivil(int y, unsigned m, unsigned d) {
            y -= m <= 2;
            const int era = (y >= 0 ? y : y - 399) / 400;
            const unsigned yoe = static_cast<unsigned>(y - era * 400);             // [0, 399]
            const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;  // [0, 365]
            const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0, 146096]
            return era * 146097 + static_cast<int>(doe) - 719468;
        }
    };
}