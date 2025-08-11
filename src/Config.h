// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Julio Cesar Ziviani Alvarez

#pragma once

namespace litevnaserver {
    using namespace std;
    using namespace makeland;

    class Config {
    public:
        string version = "1.0.0";
        int tcpPort = 0;
        string comPort;
        string loggerFile;

        Config() = default;
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        Config(const Config&&) = delete;
        Config& operator=(const Config&&) = delete;
        ~Config() = default;

        Result initialize(int argc, char* argv[]) {
            Result result = parseArgs(argc, argv);

            if (result) {
                return result;
            }
            return Result::ok();
        }

    private:
        Result parseArgs(int argc, char* argv[]) {
            int i = 1;
            comPort.clear();
            tcpPort = 0;

            while (i < argc) {
                vector<string> optionValue = su::split(argv[i], '=', false);

                if (optionValue[0] == "-com-port") {
                    if (optionValue.size() < 2) {
                        return Result("argument_error", "Option `-com-port` requires a value. Try `litevnaserver --help`");
                    }
                    comPort = optionValue[1];

                    if (comPort.size() == 0) {
                        return  Result("argument_error", "Invalid com port `{}`", comPort);
                    }
                }
                else if (optionValue[0] == "-tcp-port") {
                    if (optionValue.size() < 2) {
                        return Result("argument_error", "Option `-tcp-port` requires a value. Try `litevnaserver --help`");
                    }
                    string port = optionValue[1];
                    tcpPort = atoi(port.data());

                    if (tcpPort == 0) {
                        return  Result("argument_error", "Invalid tcp port `{}`", port);
                    }
                }
                else if (optionValue[0] == "-logger-categories") {
                    if (optionValue.size() < 2) {
                        return Result("argument_error", "Option `-logger-categories` requires a value. Try `litevnaserver --help`");
                    }
                    uint64_t categories = 0;

                    vector<string> values = su::split(optionValue[1], ',', true);

                    for (auto category : values) {
                        if (category == "info") {
                            categories |= Logger_Category_Info;
                        }
                        else if (category == "error") {
                            categories |= Logger_Category_Error;
                        }
                        else if (category == "debug") {
                            categories |= Logger_Category_Debug;
                        }
                        else if (category == "lite_vna") {
                            categories |= Logger_Category_LiteVNA;
                        }
                        else if (category == "http_server") {
                            categories |= Logger_Category_HTTPServer;
                        }
                        else if (category == "all") {
                            categories |= Logger_Category_All;
                        }
                        else {
                            return Result("argument_error", "Category `{}` is invalid. Try `litevnaserver --help`", category);
                        }
                    }
                    Logger::setCategories(categories);
                }
                else if (optionValue[0] == "-logger-file") {
                    if (optionValue.size() < 2) {
                        return Result("argument_error", "Option `-logger-file` requires a value. Try `litevnaserver --help`");
                    }
                    loggerFile = optionValue[1];
                }
                else if (optionValue[0] == "--version") {
                    return Result("version_requested", "litevnaserver {}\nLicense: GPL 2.0 only", version);
                }
                else if (optionValue[0] == "--help") {
                    return helpRequested();
                }
                else {
                    return Result("argument_error", "Option `{}` is invalid. Try `litevnaserver --help`", optionValue[0]);
                }
                i++;
            }
            if (comPort.size() == 0) {
                return Result("argument_error", "Missing `-com-port` option. Try `litevnaserver --help`");
            }
            if (tcpPort == 0) {
                return Result("argument_error", "Missing `-tcp-port` option. Try `litevnaserver --help`");
            }
            return Result::ok();
        }

        Result helpRequested() {
            return Result("help_requested",
                R"(
DESCRIPTION

    LiteVNAServer is an HTTP server for querying data (in JSON format) from a LiteVNA 64 device.
    It uses device calibration data.

USAGE

    litevnaserver [options]

    Options:
        --version                    Show version information.
        --help                       Display this information.
        -com-port=<name>             (required) serial port where LiteVNA device is connected.
        -tcp-port=<number>           (required) tcp port where LiteVNAServer will listen for requests.
        -logger-categories=<options> Comma separated options: http_server,lite_vna,info,error,all (default info,error).
        -logger-file=<file-name>     Logger output file (do not write to file by default).

    Example:
        litevnaserver -com-port={} -tcp-port=8888 -logger-categories=lite_vna,info,error

REQUEST

    Clients must send an HTML GET request with url containing all the following parameters:
        start     sweep start frequency in Hz.
        step      sweep step frequency in Hz.
        points    number of sweep frequency points.

    Example:
        http://localhost:8888/litevna?start=4300000000&step=10000000&points=2


RETURN VALUE

    For a successful call, returns a JSON with a "result" field containing the scanned data.

    Example:

    {
      "result": [
        {
          "freq": 4300000000,
          "s11": {
            "log_mag": -11.0299,
            "phase": -159.707,
            "swr": 1.78113
          },
          "s21": {
            "log_mag": -73.0412,
            "phase": -121.084
          }
        },
        {
          "freq": 4310000000,
          "s11": {
            "log_mag": -11.2397,
            "phase": -161.013,
            "swr": 1.75546
          },
          "s21": {
            "log_mag": -67.4901,
            "phase": -88.1427
          }
        }
      ]
    }

    If an error occurs, returns a JSON with an "error" field with a description.

    Example:

    {
        "error": "missing 'start' parameter"
    }
)",
#ifdef _WIN32
"COM2"
#elif __linux__
"/dev/ttyS0"
#endif
);
        }
    };
}