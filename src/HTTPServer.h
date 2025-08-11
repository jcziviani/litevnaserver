// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Julio Cesar Ziviani Alvarez

#pragma once

#include "lib/SocketTCP.h"

namespace litevnaserver {
    class HTTPServer {
    public:
        // 1. Lifecycle
        HTTPServer() = default;
        HTTPServer(const HTTPServer&) = delete;
        HTTPServer& operator=(const HTTPServer&) = delete;
        HTTPServer(const HTTPServer&&) = delete;
        HTTPServer& operator=(const HTTPServer&&) = delete;
        ~HTTPServer() = default;

        Result initialize() {
            Result result = socket->initialize();

            if (result) {
                return result;
            }

            result = socket->listen(config->tcpPort);

            if (result) {
                return result;
            }

            socket->onRead([this](uint64_t socketId, size_t totalAvailable, void* /*customData*/) {
                this->onRead(socketId, totalAvailable);
            });

            LOGGER(Info, "HTTP server listening at tcp port {}", config->tcpPort);

            return Result::ok();
        }

        void terminate() {
            socket->terminate();
        }

        // 2. Dependency injection
        void setConfig(Config* _config) {
            config = _config;
        }

        void setLiteVNA(LiteVNA* _liteVNA) {
            liteVNA = _liteVNA;
        }

        // 3. Functionalities
        Result run() {
            Result result;

            while (!result) {
                result = socket->select(100, nullptr);
            }
            return result;
        }

    private:
        LiteVNA* liteVNA = nullptr;
        Config* config = nullptr;
        unique_ptr<SocketTCP> socket = make_unique<SocketTCP>();

        void onRead(uint64_t socketId, size_t totalAvailable) {
            char* buffer = new char[totalAvailable];
            size_t totalRead = 0;

            Result result = socket->receive(socketId, buffer, totalAvailable, false, &totalRead);

            if (result) {
                LOGGER(Error, "Error reading tcp socket: {}", result.toLog());
                delete[] buffer;
                return;
            }
            string received(buffer, totalRead);
            delete[] buffer;

            LOGGER(HTTPServer, "Request received (socket_id={}): {}", socketId, received);

            vector<string> lines = su::split(received, '\r', true);

            if (lines.size() < 1) {
                write(socketId, "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request");
                return;
            }
            vector<string> request = su::split(lines[0], ' ', true);

            if (request.size() < 2 || request[0] != "GET") {
                write(socketId, "HTTP/1.1 405 Not Allowed\r\nContent-Length: 11\r\n\r\nNot Allowed");
                return;
            }
            vector<string> url = su::split(request[1], '?', false);

            if (url.size() < 1 || url[0] != "/litevna") {
                write(socketId, "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found");
                return;
            }
            if (url.size() < 2) {
                write(socketId, "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request");
                return;
            }
            vector<string> sParams = su::split(url[1], '&', false);
            unordered_map<string, string> params;

            for (auto param : sParams) {
                vector<string> keyValue = su::split(param, '=', false);

                if (keyValue.size() == 2) {
                    params.emplace(keyValue[0], keyValue[1]);
                }
            }
            string json = execute(params);
            string response = su::format("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{}", json.size(), json);

            write(socketId, response);
        }

        void write(uint64_t socketId, const string& text) {
            LOGGER(HTTPServer, "Sending response (socket_id={}): {}\n", socketId, text);

            char* data = new char[text.size()];
            memcpy(data, text.data(), text.size());

            socket->write(socketId, data, text.size(), data, [](Result result, uint64_t socketId, void* customData) {
                delete[](char*)customData;
            });
        }

        string execute(unordered_map<string, string> params) {
            auto startParam = params.find("start");

            if (startParam == params.end()) {
                return R"({"error": "missing 'start' parameter"})";
            }
            bool error;
            uint64_t start = su::atou<uint64_t>(startParam->second.data(), startParam->second.size(), &error);

            if (error || start == 0) {
                return R"({"error": "invalid 'start' parameter"})";
            }
            auto stepParam = params.find("step");

            if (stepParam == params.end()) {
                return R"({"error": "missing 'step' parameter"})";
            }
            uint64_t step = su::atou<uint64_t>(stepParam->second.data(), stepParam->second.size(), &error);

            if (error || step == 0) {
                return R"({"error": "invalid 'step' parameter"})";
            }
            auto pointsParam = params.find("points");

            if (pointsParam == params.end()) {
                return R"({"error": "missing 'points' parameter"})";
            }
            uint16_t points = su::atou<uint16_t>(pointsParam->second.data(), pointsParam->second.size(), &error);

            if (error || points == 0) {
                return R"({"error": "invalid 'points' parameter"})";
            }
            ScanValues values;
            Result result = liteVNA->scan(start, step, points, values);

            if (result) {
                return su::format(R"("{error": "{}"})", result.description);
            }
            string json = R"({"result":[)";
            bool addComma = false;
            uint64_t freq = start;

            for (size_t n = 0; n < points; n++) {
                complex<float> s11 = values.channel0In[n];
                complex<float> s21 = values.channel1In[n];

                if (addComma) {
                    json += ',';
                }
                json += su::format(R"({"freq": {}, "s11": {"log_mag": {}, "phase": {}, "swr": {}},"s21": {"log_mag": {}, "phase": {}}})",
                    freq, LiteVNA::logMag(s11), LiteVNA::phase(s11), LiteVNA::swr(s11), LiteVNA::logMag(s21), LiteVNA::phase(s21));

                freq += step;
                addComma = true;
            }
            json += "]}";

            return json;
        }
    };
}