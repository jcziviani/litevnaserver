// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#include "StringUtils.h"

namespace makeland {
    using namespace std;

    class Result {
    public:
        string code;
        string description = "ok";

        Result() = default;

        template<typename... Args>
        Result(string _code, const char* _description, Args... args) {
            code = _code;
            description = su::format(_description, args...);
        }

        string toLog() const {
            return su::format("result_code: `{}`, description: {}", code, description);
        }

        string toMessage() const {
            return su::format("result_code: `{}`\ndescription: {}", code, description);
        }

        operator bool() const {
            return code.size() > 0;
        }

        static Result ok() {
            return Result();
        }
    };
}