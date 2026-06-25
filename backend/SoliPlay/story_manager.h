// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>

struct StoryContext {
    std::string currentStoryId;
    std::string currentMode = "lightnovel";
    std::string novelStyle = "none";
    std::string audience = "none";
    std::string focus = "none";
    std::vector<std::string> currentTaboos;
    int storyRandomness = 50;
    int characterRandomness = 50;
    bool useProForActors = false;
    std::string storyTitle;
    std::string worldOverview;
    nlohmann::json protagonistData;
    std::unordered_map<std::string, nlohmann::json> characterProfiles;
    std::unordered_map<std::string, std::string> characterIdMap;
    std::unordered_map<std::string, std::string> characterDisplayNames;
    nlohmann::json directorHistory;
    std::unordered_map<std::string, nlohmann::json> characterHistories;
    nlohmann::json chatMessages = nlohmann::json::array();

    bool nextCallJsonMode = false;
    bool useStrictJson = false;

    nlohmann::json worldFacts = nlohmann::json::array();
    std::string protagonistAvatar = "/asset/profile/default.png";

    bool isResurrecting = false;
    bool useNarrator = true;   // 新增：是否启用转述者（默认开启）

    std::unordered_map<std::string, int> characterLastAppearance;

    void SaveStoryData(const std::string& dataDir);
    bool LoadStoryData(const std::string& dataDir, const std::string& storyId);
    void Reset();

    void UpdateCharacterCards(const std::string& dataDir,
        std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek,
        const std::vector<std::string>& specificHashIds = {});
};