// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "sse_handler.h"

std::mutex sseMutex;
std::function<void(const std::string&)> ssePush;

void SSESend(const std::string& data) {
    std::lock_guard<std::mutex> lock(sseMutex);
    if (ssePush) ssePush(data);
}