// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <httplib.h>
#include <string>
#include <functional>
#include "story_manager.h"

void RegisterStreamRoutes(httplib::Server& svr, StoryContext& ctx);
void RegisterStoryRoutes(httplib::Server& svr, const std::string& dataDir, StoryContext& ctx,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek);
void RegisterAvatarRoutes(httplib::Server& svr, const std::string& avatarDir, StoryContext& ctx,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek,
    const std::string& dataDir);
void RegisterTurnRoutes(httplib::Server& svr, const std::string& dataDir, StoryContext& ctx,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek);