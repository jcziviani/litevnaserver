// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#include <chrono>
#include <deque>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "DateTime.h"
#include "StringUtils.h"
#include "FileUtils.h"

#define LOGGER(category, ...)                                                                                     \
    if (makeland::Logger::hasCategory(makeland::Logger_Category_##category)) {                                    \
        makeland::Logger::instance.print(makeland::Logger_Category_##category, __FILE__, __LINE__, __VA_ARGS__);  \
    }


namespace makeland {
    using namespace std;

    static const uint64_t Logger_Category_All = (uint64_t)-1;
    static const uint64_t Logger_Category_Error = 1 << 0;
    static const uint64_t Logger_Category_Info = 1 << 1;
    static const uint64_t Logger_Category_Debug = 1 << 2;

    class Logger {
    public:
        static Logger instance;

        Logger() = default;
        Logger(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger& operator=(Logger&&) = delete;

        virtual ~Logger() = default;

        virtual void setNext(Logger* _next) {
            this->next = _next;
        }

        static void showSource(bool b) {
            sourceVisible = b;
        }

        static void addDescription(uint64_t category, const string& description) {
            descriptions.emplace(make_pair(category, description));
        }

        static void setCategories(uint64_t _categories) {
            categories = _categories;
        }

        static void setCategory(uint64_t category, bool showMessage = true) {
            categories |= category;

            if (showMessage) {
                LOGGER(Info, su::format("Setting logger_category=`{}`", getDescription(category)));
            }
        }

        static void resetCategory(uint64_t category, bool showMessage = true) {
            categories &= ~category;

            if (showMessage) {
                LOGGER(Info, su::format("Resetting logger_category=`{}`", getDescription(category)));
            }
        }

        static void clearAllCategories() {
            categories = 0;
        }

        static uint64_t getCategories() {
            return categories;
        }

        static string getDescription(uint64_t category) {
            auto it = descriptions.find(category);

            if (it != descriptions.end()) {
                return it->second;
            }
            return "";
        }

        static string describeCategories() {
            string ret;
            string comma;

            for (int n = 0; n < 63; n++) {
                uint64_t category = (uint64_t)(1LL << n);

                if (categories & category && getDescription(category).size() > 0) {
                    ret += comma + getDescription(category);
                    comma = ",";
                }
            }
            return ret;
        }

        static bool hasCategory(uint64_t category) {
            return categories & category;
        }

        template<typename... Args>
        void print(uint64_t category, const char* file, int line, const char* format, Args... args) {
            print(category, file, line, su::format(format, args...));
        }

        void print(uint64_t category, const char* file, int line, const string& message) {
            if (sourceVisible) {
                _print(category, su::format("{}({}): {}", fu::getFileName(file), line, message));
                return;
            }
            _print(category, message);
        }

        static int getUtcOffset() {
            return utcOffsetMinutes;
        }

        static void setUtcOffset(int _utcOffsetMinutes) {
            utcOffsetMinutes = _utcOffsetMinutes;
        }

        Result initialize() {
            if (commitDelayMs > 0) {
                try {
                    loggerThread = thread(&Logger::run, this);
                    loggerThread.detach();
                }
                catch (system_error& e) {
                    return Result("could_not_create_thread", "could not create thread  `logger`: {}", e.what());
                }
            }
            initialized = true;

            return Result::ok();
        }

        void terminate() {
            if (!initialized) {
                return;
            }
            requestTerminate = true;

            this_thread::sleep_for(chrono::milliseconds(commitDelayMs > 0 ? commitDelayMs * 2 : 300));

            LOGGER(Info, "Stopping logger, bye");

            flush();
        }

        void setCommitDelay(size_t _commitDelayMs) {
            if (commitDelayMs == _commitDelayMs) {
                return;
            }
            commitDelayMs = _commitDelayMs;

            if (commitDelayMs > 0 && !running) {
                try {
                    loggerThread = thread(&Logger::run, this);
                    loggerThread.detach();
                }
                catch (system_error& e) {
                    Result result("could_not_create_thread", "could not create thread  `logger`: {}", e.what());

                    fprintf(stderr, "%s\n", result.description.data());
                }
            }
        }

    protected:
        Logger* next = nullptr;

        static uint64_t categories;
        static unordered_map<uint64_t, string> descriptions;
        static bool sourceVisible;
        static int utcOffsetMinutes;

        void _print(uint64_t category, const string& message) {
            string fullMessage = su::format("{} [{}] {}\n", DateTime(DateTime::nowMicroseconds()).toString(utcOffsetMinutes), getDescription(category), message);

            if (commitDelayMs == 0) {
                try {
                    Logger* it = next;
                    vector<string> messages;
                    messages.push_back(fullMessage);

                    while (it) {
                        it->out(messages);
                        it = it->next;
                    }
                }
                catch (...) {
                    fprintf(stderr, "Could not print to logger\n");
                }
                return;
            }
            queueMutex.lock();

            if (queueMessages.size() > 100000) {
                fprintf(stderr, "Logger buffer full. Could not print message %s\n", fullMessage.data());
                queueMutex.unlock();
                return;
            }
            queueMessages.push_front(fullMessage);
            queueMutex.unlock();
        }

        virtual void out(const vector<string>& /*messages*/) {
        }

    private:
        size_t commitDelayMs = 0;
        bool initialized = false;
        bool running = false;
        bool requestTerminate = false;
        thread loggerThread;
        mutex queueMutex;
        deque<string> queueMessages;

        void run() {
            running = true;

            while (!requestTerminate) {
                flush();
                this_thread::sleep_for(chrono::milliseconds(commitDelayMs));
            }
            running = false;
        }

        void flush() {
            while (!queueMessages.empty()) {
                vector<string> messages;

                queueMutex.lock();

                while (!queueMessages.empty()) {
                    messages.push_back(queueMessages.back());
                    queueMessages.pop_back();
                }
                queueMutex.unlock();

                if (messages.size()) {
                    try {
                        Logger* it = next;

                        while (it) {
                            it->out(messages);
                            it = it->next;
                        }
                    }
                    catch (...) {
                        fprintf(stderr, "Could not print to logger\n");
                    }
                }
            }
        }
    };

    uint64_t Logger::categories = Logger_Category_Error | Logger_Category_Info;
    unordered_map<uint64_t, string> Logger::descriptions = { {Logger_Category_Error, "error"}, {Logger_Category_Info, "info"},  {Logger_Category_Debug, "debug"} };
    Logger Logger::instance;
    bool Logger::sourceVisible = false;
    int Logger::utcOffsetMinutes = 0;
}
