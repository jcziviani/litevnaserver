// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Julio Cesar Ziviani Alvarez

#pragma once

#include "lib/Logger.h"

namespace makeland {
    static const uint64_t Logger_Category_LiteVNA = 1 << 3;
    static const uint64_t Logger_Category_HTTPServer = 1 << 4;

    class LoggerLiteVNAServer {
    public:
        static void initialize() {
            Logger::addDescription(Logger_Category_LiteVNA, "lite_vna");
            Logger::addDescription(Logger_Category_HTTPServer, "http_server");

            Logger::instance.initialize();
        }
    };
}