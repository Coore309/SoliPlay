// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "story_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include <direct.h>
#include <algorithm>
#include "utils.h"
#include "debug.h"

std::function<std::string(const std::string&, const nlohmann::json&)> g_card_callDeepSeek;

static const std::string cardSystemPrompt = u8R"||(你是一个角色资料卡生成器。请根据提供的角色档案和对话历史，严格按以下格式输出该角色的最新状态（纯文本，每行一个字段）：
-------------------------------------
角色名: <真名>
性别: <性别>
角色形象: <详细外貌>
背景: <完整背景>
在 story 中的经历: <在本故事中的经历摘要>
说话特点: <说话风格与习惯>
对主角（主角名）的关系、态度的整个历程: <从初遇至今的关系发展>
当下对主角的感受: <当前的内心感受>
对其他人的关系、态度的整个历程: <与其他角色的互动历史>
物品: <当前携带的物品，逗号分隔>
能力数值: 力量:<值>, 敏捷:<值>, 智力:<值>, 体力:<值>, 魅力:<值>
技能: <技能名><值>, ...
特殊能力: <特殊能力描述>
-------------------------------------
所有信息必须基于提供的档案和历史，不得编造。如果某字段缺失，请填写“未知”。)||";

void StoryContext::UpdateCharacterCards(const std::string& dataDir,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek,
    const std::vector<std::string>& specificHashIds)
{
    if (!callDeepSeek || currentStoryId.empty()) return;
    std::string charsDir = dataDir + "\\stories\\" + currentStoryId + "\\characters";
    _mkdir(charsDir.c_str());

    std::vector<std::string> idsToProcess;
    if (!specificHashIds.empty()) {
        idsToProcess = specificHashIds;
    }
    else {
        for (const auto& pair : characterProfiles) {
            idsToProcess.push_back(pair.first);
        }
    }

    for (const auto& hashId : idsToProcess) {
        auto it = characterProfiles.find(hashId);
        if (it == characterProfiles.end()) continue;
        const nlohmann::json& profile = it->second;

        if (profile.value("true_name", "") == u8"未知" ||
            profile.value("true_name", "") == u8"某人" ||
            profile.value("background", "") == u8"临时角色，详细背景未知。")
            continue;

        try {
            std::string cardPath = charsDir + "\\" + hashId;
            std::string protagonistName = protagonistData.value("name", u8"主角");

            nlohmann::json messages = nlohmann::json::array();
            messages.push_back({ {"role", "system"}, {"content", cardSystemPrompt} });
            std::string userContent = u8"请生成角色 " + profile.value("true_name", u8"未知") + u8" 的资料卡。\n";
            userContent += u8"主角名：" + protagonistName + u8"\n";
            userContent += u8"角色档案：\n" + profile.dump(2) + u8"\n";
            if (characterHistories.find(hashId) != characterHistories.end()) {
                userContent += u8"对话历史摘要：\n" + characterHistories[hashId].dump(2) + u8"\n";
            }
            messages.push_back({ {"role", "user"}, {"content", userContent} });

            std::string cardContent = callDeepSeek("deepseek-v4-pro", messages);
            cardContent = CleanUTF8(cardContent);

            if (!cardContent.empty()) {
                std::ofstream out(cardPath, std::ios::out | std::ios::binary);
                if (out.is_open()) {
                    out << cardContent;
                    out.close();
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << u8"[角色卡] 生成失败 for " << hashId << u8": " << e.what() << std::endl;
        }
    }
}

void StoryContext::SaveStoryData(const std::string& dataDir) {
    if (currentStoryId.empty()) return;
    std::string storyDir = dataDir + "\\stories\\" + currentStoryId;
    _mkdir(storyDir.c_str());

    nlohmann::json meta;
    meta["id"] = currentStoryId;
    meta["last_played"] = std::time(nullptr);
    if (!storyTitle.empty()) {
        meta["title"] = storyTitle;
    }
    else if (directorHistory.size() > 1) {
        std::string firstUserMsg = directorHistory[1]["content"];
        meta["title"] = firstUserMsg.size() > 30 ? firstUserMsg.substr(0, 30) + "..." : firstUserMsg;
    }
    else {
        meta["title"] = "未命名故事";
    }
    nlohmann::json settings;
    settings["mode"] = currentMode;
    settings["novelStyle"] = novelStyle;
    settings["audience"] = audience;
    settings["focus"] = focus;
    settings["storyRandomness"] = storyRandomness;
    settings["characterRandomness"] = characterRandomness;
    settings["useProForActors"] = useProForActors;
    settings["taboos"] = currentTaboos;
    settings["useStrictJson"] = useStrictJson;
    settings["protagonistAvatar"] = protagonistAvatar;
    settings["useNarrator"] = useNarrator;   // 持久化转述者开关
    meta["settings"] = settings;

    // 保存回合计数（用于退场检测）
    static int turnCounter = 0;
    turnCounter++;
    meta["turn_counter"] = turnCounter;

    std::ofstream m(storyDir + "\\meta.json");
    m << meta.dump(2); m.close();

    std::ofstream f(storyDir + "\\protagonist.json");
    f << protagonistData.dump(2); f.close();

    f.open(storyDir + "\\world_overview.json");
    f << worldOverview; f.close();

    f.open(storyDir + "\\characters.json");
    nlohmann::json chars;
    for (auto& pair : characterProfiles) {
        nlohmann::json prof = pair.second;
        if (!prof.contains("original_id")) {
            for (auto& idPair : characterIdMap) {
                if (idPair.second == pair.first) {
                    prof["original_id"] = idPair.first;
                    break;
                }
            }
        }
        chars[pair.first] = prof;
    }
    f << chars.dump(2); f.close();

    f.open(storyDir + "\\director_history.json");
    f << directorHistory.dump(2); f.close();

    f.open(storyDir + "\\character_histories.json");
    nlohmann::json histories;
    for (auto& pair : characterHistories) histories[pair.first] = pair.second;
    f << histories.dump(2); f.close();

    f.open(storyDir + "\\display_names.json");
    nlohmann::json names;
    for (auto& pair : characterDisplayNames) names[pair.first] = pair.second;
    f << names.dump(2); f.close();

    f.open(storyDir + "\\world_facts.json");
    f << worldFacts.dump(2); f.close();

    f.open(storyDir + "\\chat_messages.json");
    f << chatMessages.dump(2); f.close();

    DEBUG_LOG(u8"[保存] 故事数据已写入 " << storyDir << "\n");

    // 退场检测：角色连续3回合未出场，则生成角色卡
    if (g_card_callDeepSeek) {
        std::vector<std::string> retiredIds;
        for (const auto& pair : characterProfiles) {
            const std::string& hashId = pair.first;
            const nlohmann::json& profile = pair.second;
            if (profile.value("true_name", "") == u8"未知" ||
                profile.value("true_name", "") == u8"某人" ||
                profile.value("background", "") == u8"临时角色，详细背景未知。")
                continue;

            int lastAppear = characterLastAppearance[hashId];
            if (turnCounter - lastAppear >= 3) {
                retiredIds.push_back(hashId);
            }
        }
        if (!retiredIds.empty()) {
            UpdateCharacterCards(dataDir, g_card_callDeepSeek, retiredIds);
            for (const auto& id : retiredIds) {
                characterLastAppearance[id] = turnCounter;
            }
        }
    }
}

bool StoryContext::LoadStoryData(const std::string& dataDir, const std::string& storyId) {
    std::string storyDir = dataDir + "\\stories\\" + storyId;
    std::ifstream m(storyDir + "\\meta.json");
    if (!m.is_open()) return false;
    nlohmann::json meta;
    m >> meta;
    m.close();

    {
        std::ifstream p(storyDir + "\\protagonist.json");
        if (p.is_open()) { p >> protagonistData; p.close(); }
    }
    {
        std::ifstream w(storyDir + "\\world_overview.json");
        if (w.is_open()) {
            std::stringstream buffer;
            buffer << w.rdbuf();
            worldOverview = buffer.str();
            w.close();
        }
    }
    {
        std::ifstream c(storyDir + "\\characters.json");
        if (c.is_open()) {
            nlohmann::json chars;
            c >> chars;
            characterProfiles.clear();
            characterIdMap.clear();
            for (auto& pair : chars.items()) {
                std::string hashId = pair.key();
                nlohmann::json prof = pair.value();
                characterProfiles[hashId] = prof;
                std::string origId = prof.value("original_id", "");
                if (origId.empty()) {
                    origId = prof.value("character_id", hashId);
                }
                if (!origId.empty()) {
                    characterIdMap[origId] = hashId;
                }
            }
            c.close();
        }
    }
    {
        std::ifstream d(storyDir + "\\director_history.json");
        if (d.is_open()) {
            d >> directorHistory;
            d.close();
            nlohmann::json cleanDir = nlohmann::json::array();
            for (auto& msg : directorHistory) {
                if (msg.is_object() && msg.contains("role") && msg.contains("content")) {
                    msg["content"] = CleanUTF8(msg["content"].get<std::string>());
                    cleanDir.push_back(msg);
                }
                else if (msg.is_array()) {
                    nlohmann::json obj;
                    for (const auto& element : msg) {
                        if (element.is_array() && element.size() == 2) {
                            obj[element.at(0).get<std::string>()] = element.at(1).get<std::string>();
                        }
                    }
                    if (obj.contains("role") && obj.contains("content")) {
                        obj["content"] = CleanUTF8(obj["content"].get<std::string>());
                        cleanDir.push_back(obj);
                    }
                }
            }
            directorHistory = cleanDir;
        }
    }
    {
        std::ifstream h(storyDir + "\\character_histories.json");
        if (h.is_open()) {
            nlohmann::json hist;
            h >> hist;
            characterHistories.clear();
            for (auto& el : hist.items()) {
                std::string key = el.key();
                nlohmann::json value = el.value();
                nlohmann::json clean = nlohmann::json::array();
                for (auto& msg : value) {
                    if (msg.is_object() && msg.contains("role") && msg.contains("content")) {
                        msg["content"] = CleanUTF8(msg["content"].get<std::string>());
                        clean.push_back(msg);
                    }
                    else if (msg.is_array()) {
                        nlohmann::json obj;
                        for (const auto& element : msg) {
                            if (element.is_array() && element.size() == 2) {
                                obj[element.at(0).get<std::string>()] = element.at(1).get<std::string>();
                            }
                        }
                        if (obj.contains("role") && obj.contains("content")) {
                            obj["content"] = CleanUTF8(obj["content"].get<std::string>());
                            clean.push_back(obj);
                        }
                    }
                }
                characterHistories[key] = clean;
            }
            h.close();
        }
    }
    {
        std::ifstream n(storyDir + "\\display_names.json");
        if (n.is_open()) {
            nlohmann::json names;
            n >> names;
            characterDisplayNames.clear();
            for (auto& el : names.items()) {
                characterDisplayNames[el.key()] = el.value().get<std::string>();
            }
            n.close();
        }
    }
    {
        std::ifstream wf(storyDir + "\\world_facts.json");
        if (wf.is_open()) {
            wf >> worldFacts;
            wf.close();
        }
        else {
            worldFacts = nlohmann::json::array();
        }
    }
    {
        std::ifstream cm(storyDir + "\\chat_messages.json");
        if (cm.is_open()) {
            try {
                cm >> chatMessages;
                if (!chatMessages.is_array()) chatMessages = nlohmann::json::array();
            }
            catch (...) {
                chatMessages = nlohmann::json::array();
            }
            cm.close();
        }
        else {
            chatMessages = nlohmann::json::array();
        }
    }

    currentStoryId = storyId;
    storyTitle = meta.value("title", "");

    if (meta.contains("settings")) {
        auto& sets = meta["settings"];
        currentMode = sets.value("mode", "lightnovel");
        novelStyle = sets.value("novelStyle", "none");
        audience = sets.value("audience", "none");
        focus = sets.value("focus", "none");
        storyRandomness = sets.value("storyRandomness", 50);
        characterRandomness = sets.value("characterRandomness", 50);
        useProForActors = sets.value("useProForActors", false);
        if (sets.contains("taboos")) {
            currentTaboos.clear();
            for (auto& t : sets["taboos"]) currentTaboos.push_back(t.get<std::string>());
        }
        useStrictJson = sets.value("useStrictJson", false);
        protagonistAvatar = sets.value("protagonistAvatar", "/asset/profile/default.png");
        useNarrator = sets.value("useNarrator", true);   // 读取转述者开关
    }

    SaveStoryData(dataDir);
    return true;
}

void StoryContext::Reset() {
    currentStoryId.clear();
    currentMode = "lightnovel";
    novelStyle = "none";
    audience = "none";
    focus = "none";
    currentTaboos.clear();
    storyRandomness = 50;
    characterRandomness = 50;
    useProForActors = false;
    storyTitle.clear();
    worldOverview.clear();
    protagonistData.clear();
    characterProfiles.clear();
    characterIdMap.clear();
    characterDisplayNames.clear();
    directorHistory.clear();
    characterHistories.clear();
    chatMessages.clear();
    nextCallJsonMode = false;
    useStrictJson = false;
    worldFacts = nlohmann::json::array();
    protagonistAvatar = "/asset/profile/default.png";
    isResurrecting = false;
    useNarrator = true;   // 重置为默认开启
    characterLastAppearance.clear();
}