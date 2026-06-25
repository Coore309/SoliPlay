// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <iostream>
#include <string>
#include "logger.h"

class Debug {
public:
    static bool enabled;
    static void Log(const std::string& msg);
    // 新增：记录核心日志的宏
    static void CoreInfo(const std::string& msg) { Logger::Instance().Info(msg); }
    static void CoreError(const std::string& msg) { Logger::Instance().Error(msg); }
};

#define DEBUG_LOG(x) do { if (Debug::enabled) { std::cout << x; } } while(0)