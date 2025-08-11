// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#include "DateTime.h"
#include "Logger.h"

namespace makeland {
    class LoggerConsole : public Logger {
    public:
        LoggerConsole() = default;
        LoggerConsole(const LoggerConsole&) = delete;
        LoggerConsole(LoggerConsole&&) = delete;
        LoggerConsole& operator=(const LoggerConsole&) = delete;
        LoggerConsole& operator=(LoggerConsole&&) = delete;

        void out(const vector<string>& messages) override {
            for (const string& message : messages) {
                printf("%s", message.data());
            }
        }
    };
}