// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Julio Cesar Ziviani Alvarez

#include <stdint.h>
#include <memory>

#include "lib/LoggerConsole.h"
#include "lib/LoggerFile.h"

#include "LoggerLiteVNAServer.h" 
#include "Config.h"
#include "LiteVNA.h"
#include "HTTPServer.h"

using namespace litevnaserver;
using namespace makeland;

class Main {
public:
    int execute(int argc, char* argv[]) {
        // Dependency injection
        litevna->setConfig(config.get());

        httpServer->setConfig(config.get());
        httpServer->setLiteVNA(litevna.get());

        // Initialization
        LoggerLiteVNAServer::initialize();
        Logger::instance.setNext(loggerConsole.get());

        Result result = config->initialize(argc, argv);

        if (result) {
            printf("\n%s\n", result.description.data());
            terminate();

            return -1;
        }
        if (config->loggerFile.size() > 0) {
            loggerFile->setPath(config->loggerFile);
            loggerConsole->setNext(loggerFile.get());
            LOGGER(Info, "logger_file: {}", config->loggerFile);
        }
        LOGGER(Info, "litevna2json version: {}", config->version);

        result = litevna->initialize();

        if (result) {
            LOGGER(Error, result.toLog());
            terminate();

            return -1;
        }
        result = httpServer->initialize();

        if (result) {
            LOGGER(Error, result.toLog());
            terminate();

            return -1;
        }

        // Execution
        result = httpServer->run();

        if (result) {
            LOGGER(Error, result.toLog());
            terminate();

            return -1;
        }

        // Termination
        terminate();

        return 0;
    }

    void terminate() {
        litevna->terminate();
        httpServer->terminate();
    }

private:
    unique_ptr<Logger> loggerConsole = make_unique<LoggerConsole>();
    unique_ptr<LoggerFile> loggerFile = make_unique<LoggerFile>();
    unique_ptr<Config> config = make_unique<Config>();
    unique_ptr<LiteVNA> litevna = make_unique<LiteVNA>();
    unique_ptr<HTTPServer> httpServer = make_unique<HTTPServer>();
};

int main(int argc, char* argv[]) {
    return Main().execute(argc, argv);
}