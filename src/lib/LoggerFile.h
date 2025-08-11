// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#include "DateTime.h"
#include "FileUtils.h"
#include "Logger.h"

namespace makeland {
    class LoggerFile : public Logger {
    public:
        LoggerFile() = default;
        LoggerFile(const LoggerFile&) = delete;
        LoggerFile(LoggerFile&&) = delete;
        LoggerFile& operator=(const LoggerFile&) = delete;
        LoggerFile& operator=(LoggerFile&&) = delete;

        void setName(const string _name) {
            name = _name;
        }

        void setPath(const string& _path) {
            path = _path;
            fu::mkdir(_path, true);
        }

        // TODO: Logger rotation
        void out(const vector<string>& messages) override {
            if (!file) {
                string fileName = su::format("{}/{}.log", path, name);

                file = fopen(fileName.data(), "ab");

                if (!file) {
                    fprintf(stderr, "LoggerFile fopen() error %d (%s)", errno, strerror(errno));
                    return;
                }
            }
            for (const string& message : messages) {
                if (fputs(message.data(), file) == EOF) {
                    fprintf(stderr, "LoggerFile fputs() error %d (%s)", errno, strerror(errno));
                }
            }
            fflush(file);
        }

    private:
        string path;
        string name;
        FILE* file = nullptr;
    };
}