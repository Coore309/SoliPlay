// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <functional>
#include <mutex>

extern std::mutex sseMutex;
extern std::function<void(const std::string&)> ssePush;

void SSESend(const std::string& data);