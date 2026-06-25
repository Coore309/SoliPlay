// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "api_handler.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "director_prompt.h"
#include "utils.h"
#include "dice_engine.h"
#include "sse_handler.h"
#include "debug.h"

StoryContext* g_turn_ctx = nullptr;
static std::string g_turn_dataDir;
static std::function<std::string(const std::string&, const nlohmann::json&)> g_turn_callDeepSeek;

extern std::function<std::string(const std::string&, const nlohmann::json&)> g_card_callDeepSeek;

static void sanitizeActorHistory(nlohmann::json& hist) {
    nlohmann::json clean = nlohmann::json::array();
    for (auto& msg : hist) {
        if (msg.is_object() && msg.contains("role") && msg.contains("content")) {
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
                clean.push_back(obj);
            }
        }
    }
    hist = clean;
}

static std::string BuildDirectorPrompt() {
    std::string prompt = directorBasePrompt;
    std::string modeDesc;
    if (g_turn_ctx->currentMode == "trpg") {
        modeDesc = u8"跑团模式：公正严苛，所有不确定行动必须通过D100判定，失败有真实后果，世界不围着你转。";
        modeDesc += u8" 主角属性：" + g_turn_ctx->protagonistData.dump();
    }
    else {
        modeDesc = u8"小说模式";
        if (g_turn_ctx->novelStyle != "none" && !g_turn_ctx->novelStyle.empty()) {
            if (g_turn_ctx->novelStyle == "chinese") modeDesc += u8"（中式网文";
            else if (g_turn_ctx->novelStyle == "japanese") modeDesc += u8"（日式轻小说";
        }
        if (g_turn_ctx->audience != "none" && !g_turn_ctx->audience.empty()) {
            if (g_turn_ctx->novelStyle != "none" && !g_turn_ctx->novelStyle.empty()) modeDesc += u8"，";
            else modeDesc += u8"（";
            if (g_turn_ctx->audience == "male") modeDesc += u8"男性向";
            else if (g_turn_ctx->audience == "female") modeDesc += u8"女性向";
            else if (g_turn_ctx->audience == "kids") modeDesc += u8"子供向";
            else if (g_turn_ctx->audience == "general") modeDesc += u8"一般向";
        }
        if (g_turn_ctx->focus != "none" && !g_turn_ctx->focus.empty()) {
            if ((g_turn_ctx->novelStyle != "none" && !g_turn_ctx->novelStyle.empty()) || (g_turn_ctx->audience != "none" && !g_turn_ctx->audience.empty())) modeDesc += u8"，";
            else modeDesc += u8"（";
            modeDesc += (g_turn_ctx->focus == "daily") ? u8"日常" : u8"战斗";
        }
        if (g_turn_ctx->novelStyle != "none" || g_turn_ctx->audience != "none" || g_turn_ctx->focus != "none") modeDesc += u8"）";
        modeDesc += u8"。你是故事的主角，日常行动可能失败但不会致命，关键时刻会获得戏剧性机遇，绝对避免雷点。";
        modeDesc += u8" 主角背景参考：" + g_turn_ctx->protagonistData.dump();
        if (g_turn_ctx->protagonistData.contains("specialAbility") && !g_turn_ctx->protagonistData["specialAbility"].empty())
            modeDesc += u8" 主角拥有特殊能力：" + g_turn_ctx->protagonistData["specialAbility"].get<std::string>() + u8"，可在叙事中酌情体现。";
    }
    modeDesc += u8" 故事随机性：" + std::to_string(g_turn_ctx->storyRandomness) + u8"%";
    if (g_turn_ctx->storyRandomness < 30) modeDesc += u8"（经典老套）";
    else if (g_turn_ctx->storyRandomness > 70) {
        modeDesc += u8"（新颖随机）";
        if (g_turn_ctx->currentMode != "trpg") modeDesc += u8"，但关键情节可保留经典打动人心";
    }
    else modeDesc += u8"（平衡）";
    modeDesc += u8"；人物随机性：" + std::to_string(g_turn_ctx->characterRandomness) + u8"%";
    if (g_turn_ctx->characterRandomness < 30) modeDesc += u8"（经典人设）";
    else if (g_turn_ctx->characterRandomness > 70) modeDesc += u8"（奇特个性）";
    else modeDesc += u8"（适中）";
    modeDesc += u8"。";

    std::string tabooStr;
    if (!g_turn_ctx->currentTaboos.empty()) {
        tabooStr = u8"严格避免以下雷点：";
        for (const auto& t : g_turn_ctx->currentTaboos) tabooStr += t + u8"；";
    }
    std::string worldFactsStr;
    if (!g_turn_ctx->worldFacts.empty()) {
        worldFactsStr = u8"当前已记录的世界关键事实（请严格遵循）：\n";
        for (const auto& fact : g_turn_ctx->worldFacts) {
            if (fact.is_string()) {
                worldFactsStr += fact.get<std::string>() + u8"\n";
            }
        }
    }

    size_t pos = prompt.find(u8"{{MODE_DESC}}");
    if (pos != std::string::npos) prompt.replace(pos, 14, modeDesc);
    pos = prompt.find(u8"{{TABOO_PROMPT}}");
    if (pos != std::string::npos) prompt.replace(pos, 17, tabooStr);
    pos = prompt.find(u8"{{WORLD_FACTS}}");
    if (pos != std::string::npos) prompt.replace(pos, 16, worldFactsStr);
    return prompt;
}

static const std::string narratorSystem = u8R"||(你是一个场景转述者。你的任务是把导演给出的场景提示转化为第二人称的沉浸式叙述，直接对角色说话。
规则：
- 必须使用第二人称“你”来描述角色的所见、所闻、所感、所处环境，就像你正在对角色本人讲述他/她当前的处境。
- 不要添加任何额外的评论、解释、心理描写或预设立场，只忠实地转述导演给出的信息。
- 不要使用第三人称代词（他、她、它）来指代角色，必须用“你”。
- 保持导演给出的全部细节，不得遗漏。
- 输出纯文本，不要用任何标记或格式。)||";

static std::string NarratePrompt(const std::string& directorPrompt) {
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({ {"role", "system"}, {"content", narratorSystem} });
    messages.push_back({ {"role", "user"}, {"content", u8"请转述以下场景：\n" + directorPrompt} });
    g_turn_ctx->nextCallJsonMode = false;
    std::string result = g_turn_callDeepSeek("deepseek-v4-pro", messages);
    return CleanUTF8(result);
}

static std::string NarrateCharacter(const std::string& charDetails) {
    const int maxRetries = 2;
    const std::string systemPrompt = u8"你是一个角色转述者。请将提供的角色详细档案转化为第二人称‘你’的口吻的自述。你必须以‘你是XXX...’开头，使用充满感情和细节的语言，让阅读者（即角色本身）能沉浸式地代入自己的身份。**强制要求：必须一条不落地涵盖档案中所有条目，包括角色名、性别、角色形象、背景、经历、对主角的关系历程、对其他人的关系历程、当下感受、说话特点、物品、能力数值、技能、特殊能力。不得遗漏任何一条信息。**";

    for (int retry = 0; retry <= maxRetries; ++retry) {
        nlohmann::json messages = nlohmann::json::array();
        messages.push_back({ {"role", "system"}, {"content", systemPrompt} });
        messages.push_back({ {"role", "user"}, {"content", u8"请转述以下角色档案：\n" + charDetails} });
        g_turn_ctx->nextCallJsonMode = false;
        std::string result = g_turn_callDeepSeek("deepseek-v4-pro", messages);
        result = CleanUTF8(result);
        if (!result.empty()) return result;
        if (retry < maxRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * (retry + 1)));
        }
    }

    std::string charName = u8"未知角色";
    size_t namePos = charDetails.find(u8"角色名：");
    if (namePos != std::string::npos) {
        size_t nameEnd = charDetails.find(u8"\n", namePos);
        if (nameEnd != std::string::npos) {
            charName = charDetails.substr(namePos + 4, nameEnd - namePos - 4);
        }
    }
    std::string fallback = u8"你是" + charName + u8"。\n" + u8"以下是你需要了解的关于自己的完整信息，请严格基于这些信息扮演角色：\n" + charDetails;
    return fallback;
}

static void processDialogues(const nlohmann::json& dialogues, const std::string& userContent,
    std::vector<std::string>& sceneCharacters);

static int globalTurnCounter = 0;

static bool ProcessTurn(const std::string& userContent, bool isNewStory) {
    if (!isNewStory) {
        g_turn_ctx->directorHistory.push_back({ {"role", "user"}, {"content", u8"玩家说：“" + userContent + u8"”。请继续剧情。（请确保输出合法的 JSON，格式与之前相同，动作用[]包裹，对话不加引号）"} });
        globalTurnCounter++;
    }
    else {
        globalTurnCounter = 0;
    }
    if (!isNewStory) {
        nlohmann::json userMsg = {
            {"type", "character_dialogue"},
            {"character_id", "protagonist"},
            {"name", u8"你"},
            {"text", userContent},
            {"avatar_url", g_turn_ctx->protagonistAvatar}
        };
        SSESend(userMsg.dump());
        g_turn_ctx->chatMessages.push_back(userMsg);
    }

    std::string directorReply;
    nlohmann::json directorJson;
    bool parsed = false;
    int retryCount = 0;
    const int maxRetries = 3;

    while (retryCount < maxRetries && !parsed) {
        DEBUG_LOG(u8"[导演] 第 " << (retryCount + 1) << u8" 次调用...\n");
        g_turn_ctx->nextCallJsonMode = true;
        directorReply = g_turn_callDeepSeek("deepseek-v4-pro", g_turn_ctx->directorHistory);
        g_turn_ctx->nextCallJsonMode = false;
        if (directorReply.empty()) {
            retryCount++;
            DEBUG_LOG(u8"[导演] 空回复，重试\n");
            continue;
        }
        directorReply = CleanUTF8(directorReply);
        if (Debug::enabled) {
            DEBUG_LOG(u8"[导演] 原始返回:\n" << directorReply << u8"\n");
        }
        try { directorJson = nlohmann::json::parse(directorReply); parsed = true; }
        catch (...) {
            std::string clean = directorReply;
            size_t p1 = clean.find("```json");
            if (p1 != std::string::npos) p1 += 7;
            else {
                p1 = clean.find("```");
                if (p1 != std::string::npos) p1 += 3;
                else p1 = 0;
            }
            size_t p2 = clean.rfind("```");
            if (p2 != std::string::npos && p2 > p1) clean = clean.substr(p1, p2 - p1);
            else if (p1 != 0) clean = clean.substr(p1);
            size_t objStart = clean.find('{');
            size_t objEnd = clean.rfind('}');
            if (objStart != std::string::npos && objEnd != std::string::npos && objEnd > objStart)
                clean = clean.substr(objStart, objEnd - objStart + 1);
            try { directorJson = nlohmann::json::parse(clean); parsed = true; }
            catch (...) {
                retryCount++;
                DEBUG_LOG(u8"[JSON] 第 " << retryCount << u8" 次解析失败，准备重试...\n");
                g_turn_ctx->directorHistory.push_back({ {"role", "system"}, {"content", u8"你上一次的输出不是合法的 JSON，请严格遵循 JSON 格式重新输出。"} });
            }
        }

        if (parsed && g_turn_ctx->isResurrecting && directorJson.contains("dialogues") && directorJson["dialogues"].is_array()) {
            std::string availableNames;
            for (const auto& pair : g_turn_ctx->characterDisplayNames) {
                availableNames += pair.second + u8" ";
            }
            bool hasUnauthorized = false;
            for (auto& d : directorJson["dialogues"]) {
                std::string origId = d.value("character_id", "");
                if (origId.empty()) continue;
                bool found = false;
                if (g_turn_ctx->characterIdMap.find(origId) != g_turn_ctx->characterIdMap.end()) {
                    found = true;
                }
                else {
                    for (const auto& pair : g_turn_ctx->characterDisplayNames) {
                        if (pair.second == origId) { found = true; break; }
                    }
                }
                if (!found) {
                    hasUnauthorized = true;
                    std::cerr << u8"[复活] 导演使用了未授权角色 " << origId << u8"，要求重试" << std::endl;
                    break;
                }
            }
            if (hasUnauthorized) {
                retryCount++;
                g_turn_ctx->directorHistory.push_back({ {"role", "system"}, {"content", u8"你的对话中包含了未授权的角色，请严格使用以下角色名：\n" + availableNames + u8"\n请重新输出 JSON，不要生成任何其他角色。"} });
                directorJson.clear();
                parsed = false;
                continue;
            }
        }
    }

    if (!parsed) {
        std::cerr << u8"[JSON] 重试耗尽，放弃此回合" << std::endl;
        SSESend(u8"{\"type\":\"error\",\"message\":\"导演响应格式错误，请稍后再试。\"}");
        SSESend(u8"{\"type\":\"input_required\",\"prompt\":\"请稍后重试\"}");
        SSESend(u8"{\"type\":\"done\"}");
        return false;
    }

    g_turn_ctx->directorHistory.push_back({ {"role", "assistant"}, {"content", directorReply} });
    DEBUG_LOG(u8"[导演] 解析成功\n");
    if (Debug::enabled) {
        DEBUG_LOG(u8"[导演] 解析后的 JSON:\n" << directorJson.dump(2) << u8"\n");
    }

    if (directorJson.contains("world_facts") && directorJson["world_facts"].is_array()) {
        for (auto& fact : directorJson["world_facts"]) {
            if (fact.is_string()) {
                std::string newFact = fact.get<std::string>();
                bool exists = false;
                for (auto& existing : g_turn_ctx->worldFacts) {
                    if (existing.is_string() && existing.get<std::string>() == newFact) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    g_turn_ctx->worldFacts.push_back(newFact);
                }
            }
        }
    }

    if (isNewStory && directorJson.contains("title")) {
        g_turn_ctx->storyTitle = directorJson["title"];
        DEBUG_LOG(u8"[标题] " << g_turn_ctx->storyTitle << "\n");
    }

    if (directorJson.contains("check_required") && !directorJson["check_required"].is_null()) {
        auto& check = directorJson["check_required"];
        std::string skill = check.value("skill", "");
        int difficulty = check.value("difficulty", 40);
        bool miracle = directorJson.value("miracle", false);
        CheckResult result = DiceCheck(g_turn_ctx->protagonistData, skill, difficulty);

        nlohmann::json resultMsg = {
            {"type", "action_result"},
            {"character_id", "protagonist"},
            {"skill", skill},
            {"roll", result.roll},
            {"success", result.success || miracle},
            {"miracle", miracle},
            {"description", ""}
        };
        SSESend(resultMsg.dump());
        g_turn_ctx->chatMessages.push_back(resultMsg);

        std::string feedback = (result.success || miracle) ? u8"判定成功" : u8"判定失败";
        if (miracle && !result.success) feedback += u8"（奇迹发生！）";
        g_turn_ctx->directorHistory.push_back({ {"role", "system"}, {"content", u8"判定结果：" + skill + u8" " + feedback + u8"（掷骰：" + std::to_string(result.roll) + u8"）"} });
        DEBUG_LOG(u8"[判定] " << skill << u8" " << feedback << u8" (掷骰: " << result.roll << u8")\n");

        g_turn_ctx->nextCallJsonMode = true;
        directorReply = g_turn_callDeepSeek("deepseek-v4-pro", g_turn_ctx->directorHistory);
        g_turn_ctx->nextCallJsonMode = false;
        if (!directorReply.empty()) {
            directorReply = CleanUTF8(directorReply);
            g_turn_ctx->directorHistory.push_back({ {"role", "assistant"}, {"content", directorReply} });
            try { directorJson = nlohmann::json::parse(directorReply); parsed = true; }
            catch (...) { parsed = false; }
            if (!parsed) {
                SSESend(u8"{\"type\":\"narrator\",\"text\":\"" + directorReply + u8"\"}");
                g_turn_ctx->chatMessages.push_back({ {"type", "narrator"}, {"text", directorReply} });
                SSESend(u8"{\"type\":\"input_required\",\"prompt\":\"请输入你的行动或对话...\"}");
                SSESend(u8"{\"type\":\"done\"}");
                return false;
            }
        }
    }

    if (directorJson.contains("world_overview")) g_turn_ctx->worldOverview = directorJson["world_overview"];
    if (directorJson.contains("character_profiles")) {
        DEBUG_LOG(u8"[角色] 收到 " << directorJson["character_profiles"].size() << u8" 个角色档案\n");
        for (auto& profile : directorJson["character_profiles"]) {
            std::string origId = profile.value("character_id", "");
            std::string hashId = origId + "_" + GenerateHash();
            profile["original_id"] = origId;
            profile["character_id"] = hashId;
            g_turn_ctx->characterProfiles[hashId] = profile;
            if (!origId.empty()) g_turn_ctx->characterIdMap[origId] = hashId;
            std::string displayName = profile.value("display_name", profile.value("true_name", origId));
            g_turn_ctx->characterDisplayNames[hashId] = displayName;
            if (Debug::enabled) {
                DEBUG_LOG(u8"[角色档案] " << displayName << u8":\n" << profile.dump(2) << u8"\n");
            }
        }
    }

    if (directorJson.contains("name_updates") && directorJson["name_updates"].is_array()) {
        for (auto& update : directorJson["name_updates"]) {
            std::string charId = update.value("character_id", "");
            std::string newName = update.value("display_name", "");
            if (!charId.empty() && !newName.empty()) {
                std::string hashId = g_turn_ctx->characterIdMap[charId];
                if (!hashId.empty()) {
                    g_turn_ctx->characterDisplayNames[hashId] = newName;
                }
            }
        }
    }

    std::string narrationText = directorJson.value("narration",
        directorJson.value("narrative", ""));
    if (!narrationText.empty()) {
        DEBUG_LOG(u8"[旁白] 发送\n");
        nlohmann::json narrMsg;
        narrMsg["type"] = "narrator";
        narrMsg["text"] = narrationText;
        SSESend(narrMsg.dump());
        g_turn_ctx->chatMessages.push_back(narrMsg);
    }

    std::vector<std::string> sceneCharacters;
    if (directorJson.contains("dialogues")) {
        processDialogues(directorJson["dialogues"], userContent, sceneCharacters);
    }
    else if (directorJson.contains("dialogue")) {
        nlohmann::json dialogueVal = directorJson["dialogue"];
        if (dialogueVal.is_array()) {
            processDialogues(dialogueVal, userContent, sceneCharacters);
        }
        else if (dialogueVal.is_object()) {
            processDialogues(nlohmann::json::array({ dialogueVal }), userContent, sceneCharacters);
        }
    }

    bool wait = directorJson.value("wait_for_user", true);
    std::string inputPrompt = directorJson.value("input_prompt", u8"请输入你的行动或对话...");
    DEBUG_LOG(u8"[回合] 等待用户输入, 提示: " << inputPrompt << "\n");
    nlohmann::json promptMsg;
    promptMsg["type"] = "input_required";
    promptMsg["prompt"] = inputPrompt;
    SSESend(promptMsg.dump());
    SSESend(u8"{\"type\":\"done\"}");
    g_turn_ctx->SaveStoryData(g_turn_dataDir);
    return true;
}

static void processDialogues(const nlohmann::json& dialogues, const std::string& userContent,
    std::vector<std::string>& sceneCharacters) {
    DEBUG_LOG(u8"[对话] 开始处理 " << dialogues.size() << u8" 个角色\n");
    sceneCharacters.clear();

    for (auto& d : dialogues) {
        std::string origId = d.value("character_id", "unknown");
        std::string hashId;
        if (g_turn_ctx->characterIdMap.find(origId) != g_turn_ctx->characterIdMap.end())
            hashId = g_turn_ctx->characterIdMap[origId];
        else {
            hashId = origId + "_" + GenerateHash();
            g_turn_ctx->characterIdMap[origId] = hashId;
            if (g_turn_ctx->characterProfiles.find(hashId) == g_turn_ctx->characterProfiles.end()) {
                nlohmann::json fallbackProfile;
                fallbackProfile["character_id"] = hashId;
                fallbackProfile["original_id"] = origId;

                std::string fallbackTrueName = g_turn_ctx->characterDisplayNames[hashId];
                if (fallbackTrueName.empty() || fallbackTrueName == u8"未知角色") {
                    fallbackTrueName = origId;
                }
                if (fallbackTrueName.empty()) fallbackTrueName = u8"未知角色";

                std::string fallbackDisplayName = g_turn_ctx->characterDisplayNames[hashId];
                if (fallbackDisplayName.empty() || fallbackDisplayName == u8"未知角色") {
                    fallbackDisplayName = d.value("name", u8"某人");
                    if (fallbackDisplayName.empty()) fallbackDisplayName = u8"未知角色";

                    bool nameExists = false;
                    for (const auto& pair : g_turn_ctx->characterDisplayNames) {
                        if (pair.first != hashId && pair.second == fallbackDisplayName) {
                            nameExists = true;
                            break;
                        }
                    }
                    if (nameExists) {
                        int suffix = 2;
                        std::string baseName = fallbackDisplayName;
                        while (true) {
                            std::string candidate = baseName + u8"(" + std::to_string(suffix) + u8")";
                            bool exists = false;
                            for (const auto& pair : g_turn_ctx->characterDisplayNames) {
                                if (pair.second == candidate) { exists = true; break; }
                            }
                            if (!exists) {
                                fallbackDisplayName = candidate;
                                break;
                            }
                            suffix++;
                        }
                    }
                    g_turn_ctx->characterDisplayNames[hashId] = fallbackDisplayName;
                }

                fallbackProfile["true_name"] = fallbackTrueName;
                fallbackProfile["display_name"] = fallbackDisplayName;
                fallbackProfile["background"] = u8"临时角色，详细背景未知。";
                fallbackProfile["personality"] = u8"未知";
                fallbackProfile["appearance"] = u8"未知";
                fallbackProfile["initial_attitude_to_player"] = u8"中立";
                g_turn_ctx->characterProfiles[hashId] = fallbackProfile;
                std::cerr << u8"[警告] 为角色 " << origId << u8" 自动创建了兜底档案。" << std::endl;
            }
        }
        sceneCharacters.push_back(hashId);
        g_turn_ctx->characterLastAppearance[hashId] = globalTurnCounter;
    }

    for (size_t i = 0; i < dialogues.size(); ++i) {
        auto& d = dialogues[i];
        std::string scenePrompt = d.value("prompt", "");
        std::string hashId = sceneCharacters[i];
        std::string displayName = g_turn_ctx->characterDisplayNames[hashId];
        if (displayName.empty()) displayName = d.value("name", u8"某人");
        DEBUG_LOG(u8"[演员] 准备 " << displayName << u8" (ID: " << hashId << u8")\n");
        DEBUG_LOG(u8"[演员] 场景提示: " << scenePrompt << u8"\n");

        std::string narratedPrompt;
        if (!scenePrompt.empty()) {
            if (g_turn_ctx->useNarrator) {
                narratedPrompt = NarratePrompt(scenePrompt);
            }
            else {
                narratedPrompt = scenePrompt;
            }
        }
        else {
            narratedPrompt = scenePrompt;
        }

        std::string charSystemContent;
        std::string worldFactsInfo;
        if (!g_turn_ctx->worldFacts.empty()) {
            worldFactsInfo += u8" 当前世界的已知事实（请严格遵循）：\n";
            for (const auto& fact : g_turn_ctx->worldFacts) {
                if (fact.is_string()) worldFactsInfo += fact.get<std::string>() + u8"\n";
            }
        }

        if (g_turn_ctx->characterHistories.find(hashId) == g_turn_ctx->characterHistories.end() ||
            g_turn_ctx->characterHistories[hashId].empty() ||
            g_turn_ctx->characterHistories[hashId][0].value("role", "") != "system") {
            std::string charDetails;
            if (g_turn_ctx->characterProfiles.find(hashId) != g_turn_ctx->characterProfiles.end()) {
                auto& p = g_turn_ctx->characterProfiles[hashId];
                std::string trueName = p.value("true_name", displayName);
                std::string bg = p.value("background", "");
                std::string personality = p.value("personality", "");
                std::string appearance = p.value("appearance", "");
                std::string attitude = p.value("initial_attitude_to_player", "");
                if (trueName.empty() || trueName == u8"未知") trueName = displayName;
                charDetails = u8"角色名：" + trueName + u8"\n背景：" + bg + u8"\n性格：" + personality + u8"\n外貌：" + appearance + u8"\n对主角态度：" + attitude;
            }
            else {
                std::string fallbackName = displayName;
                if (fallbackName.empty() || fallbackName == u8"未知角色") fallbackName = u8"未知角色";
                charDetails = u8"角色名：" + fallbackName + u8"\n背景：临时角色，详细背景未知。\n性格：未知\n外貌：未知\n对主角态度：中立";
            }

            std::string systemMsg;
            if (g_turn_ctx->useNarrator) {
                systemMsg = NarrateCharacter(charDetails);
            }
            else {
                systemMsg = charDetails;
            }

            systemMsg += u8"\n\n【输出规则】你的每条回复必须且仅包含以下两种内容，不能有其他任何东西：\n"
                u8"   1. 动作描写：**只能**用英文方括号 [] 包裹，方括号内绝对不能出现任何主语（如“她”、“他”、“我”、“你”、“莉露”、“林”等），只能直接描写动作。示例：[微微点头]、[握紧拳头]、[坐在地上]。\n"
                u8"   2. 对话：不加引号，直接说话。\n"
                u8"   **绝对禁止**使用全角圆括号（）、中文方括号【】、双竖线 || 来包裹动作。这些都会导致程序错误。\n"
                u8"   如果动作有明确目标，目标为其他角色时使用该角色的特征名或真名，例如：[向银发少女点点头]、[拉住林]。目标为主角时必须使用“你”，例如：[轻轻拍了拍你的肩膀]、[抓着你的手]。\n"
                u8"   特别强调：绝对不能输出“[她”这个字符组合，这会导致程序错误。\n"
                u8"   对话绝对不能使用任何引号包裹，包括中文引号「」、英文双引号\"\"、单引号''等。\n"
                u8" 【严禁事项】\n"
                u8"   - 禁止以第三人称叙述自己的行为（如“他走向门口”或“她叹了口气”）。\n"
                u8"   - 禁止描述其他角色的行为、动作或话语。你只能描述自己的行动和言语，不能替别人说话或行动。\n"
                u8"   - 禁止跳出角色进行评论、解释或心理描写（如“此时她内心充满矛盾”）。\n"
                u8"   - 禁止使用任何叙述性语言描述场景或其他角色的内心。\n"
                u8"   - 如果你发现自己正在写“他/她/它……”或“主角……”，立即停止并改为直接动作或对话。\n"
                u8" 【人称铁律】你绝不能使用第一人称（“我”“我的”）来叙述自己的行动。你的动作必须放在方括号里且不加人称。对话不加引号。其他角色发言中的“我”与你无关。\n"
                u8" 【身份防混淆】当前场景中有多个角色，每个都有不同的特征和名字。你只是其中的一位。请严格根据你的名字和背景来回应，不要替其他角色说话或行动。如果收到‘某某说：’的消息，那是别人说的话，与你无关。\n"
                u8" 注意根据当前世界背景使用合适的称呼和用词。";
            if (!g_turn_ctx->currentTaboos.empty()) {
                systemMsg += u8" 雷点：";
                for (const auto& t : g_turn_ctx->currentTaboos) systemMsg += t + u8"；";
            }

            if (g_turn_ctx->characterHistories.find(hashId) == g_turn_ctx->characterHistories.end()) {
                g_turn_ctx->characterHistories[hashId] = nlohmann::json::array();
            }
            g_turn_ctx->characterHistories[hashId].insert(g_turn_ctx->characterHistories[hashId].begin(), { {"role", "system"}, {"content", systemMsg} });
        }

        nlohmann::json& actorHist = g_turn_ctx->characterHistories[hashId];
        sanitizeActorHistory(actorHist);

        std::string fullPrompt = narratedPrompt;
        fullPrompt += u8" 下面是主角刚刚说的话（其中的“我”是主角，不是你）：「" + userContent + u8"」";
        for (size_t j = 0; j < i; ++j) {
            std::string prevHashId = sceneCharacters[j];
            auto& prevHist = g_turn_ctx->characterHistories[prevHashId];
            sanitizeActorHistory(prevHist);
            if (!prevHist.empty() && prevHist.back().is_object() && prevHist.back().value("role", "") == "assistant") {
                std::string prevName = g_turn_ctx->characterDisplayNames[prevHashId];
                std::string prevFullReply = prevHist.back().value("content", "");
                prevFullReply = CleanUTF8(prevFullReply);
                std::string pureDialogue = prevFullReply;
                size_t bracketStart;
                while ((bracketStart = pureDialogue.find(u8"[")) != std::string::npos) {
                    size_t bracketEnd = pureDialogue.find(u8"]", bracketStart);
                    if (bracketEnd == std::string::npos) break;
                    pureDialogue.erase(bracketStart, bracketEnd - bracketStart + 1);
                }
                fullPrompt += u8" " + prevName + u8"说：" + pureDialogue;
            }
        }
        fullPrompt += u8" 现在，请以" + displayName + u8"的身份直接做出回应。记住：你只能输出动作（方括号内）和对话，对话绝对不能加任何引号，动作方括号内绝对不能出现主语，尤其是“[她”。";

        actorHist.push_back({ {"role", "user"}, {"content", fullPrompt} });
        if (Debug::enabled) {
            DEBUG_LOG(u8"[演员] 完整提示:\n" << fullPrompt << u8"\n");
        }

        std::string actorModel = g_turn_ctx->useProForActors ? "deepseek-v4-pro" : "deepseek-v4-flash";
        std::string charReply = g_turn_callDeepSeek(actorModel, actorHist);
        charReply = CleanUTF8(charReply);
        DEBUG_LOG(u8"[演员] " << displayName << u8" 回复:\n" << charReply << u8"\n");

        actorHist.push_back({ {"role", "assistant"}, {"content", charReply} });

        for (auto& listenerId : sceneCharacters) {
            if (listenerId != hashId) {
                std::string listenerName = g_turn_ctx->characterDisplayNames[listenerId];
                std::string broadcastContent = displayName + u8"说：" + charReply;
                broadcastContent = CleanUTF8(broadcastContent);
                g_turn_ctx->characterHistories[listenerId].push_back(
                    { {"role", "user"}, {"content", broadcastContent} }
                );
            }
        }

        std::string avatarUrl = "/asset/profile/default.png";
        if (g_turn_ctx->characterProfiles.find(hashId) != g_turn_ctx->characterProfiles.end()) {
            avatarUrl = g_turn_ctx->characterProfiles[hashId].value("avatar_url", "/asset/profile/default.png");
        }

        nlohmann::json charMsg;
        charMsg["type"] = "character_dialogue";
        charMsg["character_id"] = hashId;
        charMsg["name"] = displayName;
        charMsg["text"] = charReply.empty() ? u8"..." : charReply;
        charMsg["avatar_url"] = avatarUrl;
        SSESend(charMsg.dump());
        g_turn_ctx->chatMessages.push_back(charMsg);
    }
}

static void HandleTurn(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string type = reqJson.value("type", "user_input");
        std::string content = reqJson.value("content", "");

        if (type == "start_story") {
            DEBUG_LOG(u8"[Turn] 开始新故事\n");
            g_turn_ctx->Reset();
            g_turn_ctx->currentStoryId = "story_" + GenerateHash();
            DEBUG_LOG(u8"[Turn] 故事ID: " << g_turn_ctx->currentStoryId << "\n");
            g_turn_ctx->currentMode = reqJson.value("mode", "lightnovel");
            g_turn_ctx->novelStyle = reqJson.value("novelStyle", "none");
            g_turn_ctx->audience = reqJson.value("audience", "none");
            g_turn_ctx->focus = reqJson.value("focus", "none");
            g_turn_ctx->storyRandomness = reqJson.value("storyRandomness", 50);
            g_turn_ctx->characterRandomness = reqJson.value("characterRandomness", 50);
            g_turn_ctx->useProForActors = reqJson.value("useProForActors", false);
            g_turn_ctx->useNarrator = reqJson.value("enable_narrator", true);  // 读取转述者开关
            if (reqJson.contains("taboos")) {
                for (auto& t : reqJson["taboos"]) g_turn_ctx->currentTaboos.push_back(t.get<std::string>());
            }
            g_turn_ctx->protagonistData = reqJson.value("protagonist", nlohmann::json::object());
            if (!g_turn_ctx->protagonistData.contains("specialAbility")) g_turn_ctx->protagonistData["specialAbility"] = "";
            if (!g_turn_ctx->protagonistData.contains("skills")) g_turn_ctx->protagonistData["skills"] = nlohmann::json::object();
            if (!g_turn_ctx->protagonistData.contains("inventory")) g_turn_ctx->protagonistData["inventory"] = nlohmann::json::array();
            if (!g_turn_ctx->protagonistData.contains("attributes")) {
                g_turn_ctx->protagonistData["attributes"] = { {"str",50},{"dex",50},{"int",50},{"con",50},{"cha",50} };
            }

            if (reqJson.value("resurrect", false)) {
                std::string storyWorldview = reqJson.value("story_worldview", "");
                std::string storyChain = reqJson.value("story_chain", "");
                std::string plotStyle = reqJson.value("plot_style", "");
                auto characters = reqJson.value("characters", nlohmann::json::array());
                g_turn_ctx->useNarrator = reqJson.value("enable_narrator", true);  // 复活时也读取

                std::vector<std::string> resurrectCharNames;

                for (auto& charObj : characters) {
                    std::string charName = charObj.value("name", "未知");
                    resurrectCharNames.push_back(charName);

                    std::string charDetails;
                    if (!charName.empty())           charDetails += u8"角色名：" + charName + u8"\n";
                    if (charObj.contains("gender") && !charObj["gender"].get<std::string>().empty())
                        charDetails += u8"性别：" + charObj["gender"].get<std::string>() + u8"\n";
                    if (charObj.contains("appearance") && !charObj["appearance"].get<std::string>().empty())
                        charDetails += u8"角色形象：" + charObj["appearance"].get<std::string>() + u8"\n";
                    if (charObj.contains("background") && !charObj["background"].get<std::string>().empty())
                        charDetails += u8"背景：" + charObj["background"].get<std::string>() + u8"\n";
                    if (charObj.contains("storyExperience") && !charObj["storyExperience"].get<std::string>().empty())
                        charDetails += u8"在story中的经历：" + charObj["storyExperience"].get<std::string>() + u8"\n";
                    if (charObj.contains("relationToMain") && !charObj["relationToMain"].get<std::string>().empty())
                        charDetails += u8"对主角的关系、态度的整个历程：" + charObj["relationToMain"].get<std::string>() + u8"\n";
                    if (charObj.contains("relationToOthers") && !charObj["relationToOthers"].get<std::string>().empty())
                        charDetails += u8"对其他人的关系、态度的整个历程：" + charObj["relationToOthers"].get<std::string>() + u8"\n";
                    if (charObj.contains("currentFeeling") && !charObj["currentFeeling"].get<std::string>().empty())
                        charDetails += u8"当下对主角的感受：" + charObj["currentFeeling"].get<std::string>() + u8"\n";
                    if (charObj.contains("speechStyle") && !charObj["speechStyle"].get<std::string>().empty())
                        charDetails += u8"说话特点：" + charObj["speechStyle"].get<std::string>() + u8"\n";
                    if (charObj.contains("inventory") && !charObj["inventory"].get<std::string>().empty())
                        charDetails += u8"物品：" + charObj["inventory"].get<std::string>() + u8"\n";
                    if (charObj.contains("attributes") && !charObj["attributes"].get<std::string>().empty())
                        charDetails += u8"能力数值：" + charObj["attributes"].get<std::string>() + u8"\n";
                    if (charObj.contains("skills") && !charObj["skills"].get<std::string>().empty())
                        charDetails += u8"技能：" + charObj["skills"].get<std::string>() + u8"\n";
                    if (charObj.contains("specialAbility") && !charObj["specialAbility"].get<std::string>().empty())
                        charDetails += u8"特殊能力：" + charObj["specialAbility"].get<std::string>() + u8"\n";

                    if (charDetails.empty()) continue;

                    std::string narratorReply;
                    if (g_turn_ctx->useNarrator) {
                        narratorReply = NarrateCharacter(charDetails);
                    }
                    else {
                        narratorReply = u8"你是" + charName + u8"。\n" + u8"以下是你需要了解的关于自己的完整信息，请严格基于这些信息扮演角色：\n" + charDetails;
                    }

                    std::string hashId = charObj.value("id", "resurrect_" + GenerateHash());
                    g_turn_ctx->characterHistories[hashId] = nlohmann::json::array();
                    g_turn_ctx->characterHistories[hashId].push_back({ {"role", "system"}, {"content", narratorReply} });

                    nlohmann::json profile;
                    profile["character_id"] = hashId;
                    profile["original_id"] = hashId;
                    profile["true_name"] = charName;
                    profile["display_name"] = charName;
                    profile["background"] = charDetails;
                    profile["personality"] = "";
                    profile["appearance"] = charObj.value("appearance", "");
                    profile["initial_attitude_to_player"] = "";
                    g_turn_ctx->characterProfiles[hashId] = profile;
                    g_turn_ctx->characterIdMap[hashId] = hashId;
                    g_turn_ctx->characterDisplayNames[hashId] = charName;

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                std::string directorSystem = u8R"(
你是 SoliPlay 的游戏导演。你的输出必须是严格的 JSON 对象。

【绝对规则】主角永远由玩家控制，你**绝对不能**在 JSON 中生成主角的对话、动作或任何直接台词。主角的言行只能由玩家输入。旁白必须使用第二人称描述环境和 NPC 行为，不能替主角做任何决定或说出任何话。
【复活角色限制】本场景**只能**使用以下角色，**绝对不能创建任何新角色**，也不能让未列出的角色发言。可用角色名单：)";

                for (const auto& name : resurrectCharNames) {
                    directorSystem += u8"\n- " + name;
                }

                directorSystem += u8R"(
在 dialogues 数组中，你必须使用角色的名字作为 character_id 的值。
这是一个从剧情断点复活的存档。请根据下面的世界观、故事链以及角色档案，生成一段衔接的旁白和对话，让故事自然地继续下去。不要重新开始，不要修改已有的角色关系，严格按照故事链的当前状态推进剧情。
)";

                directorSystem += u8"\n当前模式：" + g_turn_ctx->currentMode;
                if (!g_turn_ctx->currentTaboos.empty()) {
                    directorSystem += u8"\n严格避免以下雷点：";
                    for (const auto& t : g_turn_ctx->currentTaboos) directorSystem += t + u8"；";
                }
                if (!g_turn_ctx->worldFacts.empty()) {
                    directorSystem += u8"\n当前已记录的世界关键事实：\n";
                    for (const auto& fact : g_turn_ctx->worldFacts)
                        if (fact.is_string()) directorSystem += fact.get<std::string>() + u8"\n";
                }
                directorSystem += u8"\n世界观：\n" + storyWorldview + u8"\n\n故事链：\n" + storyChain + u8"\n\n情节安排特点：\n" + plotStyle;

                g_turn_ctx->directorHistory = nlohmann::json::array();
                g_turn_ctx->directorHistory.push_back({ {"role", "system"}, {"content", directorSystem} });
                g_turn_ctx->directorHistory.push_back({ {"role", "user"}, {"content", u8"开始新故事，要求：" + content + u8"。（请输出合法的 JSON，格式与要求一致，动作用[]包裹，对话不加引号）"} });

                g_turn_ctx->isResurrecting = true;
                ProcessTurn(content, true);
                g_turn_ctx->isResurrecting = false;
                return;
            }

            g_turn_ctx->directorHistory = nlohmann::json::array();
            g_turn_ctx->directorHistory.push_back({ {"role", "system"}, {"content", BuildDirectorPrompt()} });
            g_turn_ctx->directorHistory.push_back({ {"role", "user"}, {"content", u8"开始新故事，要求：" + content + u8"。（请输出合法的 JSON，格式与要求一致，动作用[]包裹，对话不加引号）"} });
            ProcessTurn(content, true);
        }
        else if (type == "user_input") {
            if (g_turn_ctx->directorHistory.empty()) {
                DEBUG_LOG(u8"[Turn] 无导演历史，自动创建故事\n");
                g_turn_ctx->Reset();
                g_turn_ctx->currentStoryId = "story_" + GenerateHash();
                g_turn_ctx->currentMode = "lightnovel";
                g_turn_ctx->novelStyle = "none";
                g_turn_ctx->audience = "none";
                g_turn_ctx->focus = "none";
                g_turn_ctx->storyRandomness = 50;
                g_turn_ctx->characterRandomness = 50;
                g_turn_ctx->protagonistData = nlohmann::json::object();
                g_turn_ctx->protagonistData["attributes"] = { {"str",50},{"dex",50},{"int",50},{"con",50},{"cha",50} };
                g_turn_ctx->protagonistData["skills"] = nlohmann::json::object();
                g_turn_ctx->protagonistData["inventory"] = nlohmann::json::array();
                g_turn_ctx->protagonistData["specialAbility"] = "";

                g_turn_ctx->directorHistory = nlohmann::json::array();
                g_turn_ctx->directorHistory.push_back({ {"role", "system"}, {"content", BuildDirectorPrompt()} });
                g_turn_ctx->directorHistory.push_back({ {"role", "user"}, {"content", u8"玩家说：“" + content + u8"”。请开始故事。（请输出合法的 JSON，格式与要求一致，动作用[]包裹，对话不加引号）"} });
                ProcessTurn(content, true);
            }
            else {
                ProcessTurn(content, false);
            }
        }
        else {
            DEBUG_LOG(u8"[Turn] 未知指令类型\n");
            SSESend(u8"{\"type\":\"error\",\"message\":\"未知指令\"}");
            SSESend(u8"{\"type\":\"input_required\",\"prompt\":\"未知指令\"}");
            SSESend(u8"{\"type\":\"done\"}");
        }
        res.set_content("ok", "text/plain");
    }
    catch (const std::exception& e) {
        std::cerr << u8"[异常] " << e.what() << std::endl;
        SSESend(u8"{\"type\":\"error\",\"message\":\"内部服务器错误\"}");
        SSESend(u8"{\"type\":\"input_required\",\"prompt\":\"发生错误，请重试。\"}");
        SSESend(u8"{\"type\":\"done\"}");
        res.status = 500;
        res.set_content(u8"error", "text/plain");
    }
}

static void HandleGetCharacterInfo(const httplib::Request& req, httplib::Response& res) {
    std::string characterId = req.get_param_value("character_id");
    if (characterId.empty()) {
        res.status = 400;
        res.set_content(u8"{\"error\":\"缺少 character_id\"}", "application/json");
        return;
    }
    std::string hashId = characterId;
    if (g_turn_ctx->characterProfiles.find(hashId) == g_turn_ctx->characterProfiles.end()) {
        auto it = g_turn_ctx->characterIdMap.find(characterId);
        if (it != g_turn_ctx->characterIdMap.end()) {
            hashId = it->second;
        }
        else {
            res.status = 404;
            res.set_content(u8"{\"error\":\"角色不存在\"}", "application/json");
            return;
        }
    }
    nlohmann::json info;
    info["hash_id"] = hashId;
    info["display_name"] = g_turn_ctx->characterDisplayNames[hashId];
    info["true_name"] = g_turn_ctx->characterProfiles[hashId].value("true_name", "未知");
    res.set_content(info.dump(), "application/json");
}

static void HandleGenerateAvatarPrompt(const httplib::Request& req, httplib::Response& res) {
    auto j = nlohmann::json::parse(req.body);
    std::string characterId = j["character_id"];
    std::string hashId = characterId;
    if (g_turn_ctx->characterProfiles.find(hashId) == g_turn_ctx->characterProfiles.end()) {
        res.status = 404;
        res.set_content(u8"{\"error\":\"角色不存在\"}", "application/json");
        return;
    }
    auto& profile = g_turn_ctx->characterProfiles[hashId];
    std::string name = profile.value("true_name", characterId);
    std::string appearance = profile.value("appearance", "");

    nlohmann::json messages = nlohmann::json::array({
        {{"role", "system"}, {"content", u8"You are a character image prompt generator. Based on the given character appearance description, output a concise Stable Diffusion prompt in English, using comma-separated tags only. Do NOT add any quality tags, artist names, background descriptions, or explanations. Example output: 1girl, silver hair, blue eyes, petite, white dress, barefoot"}},
        {{"role", "user"}, {"content", u8"Character: " + name + u8". Appearance: " + appearance}}
        });
    DEBUG_LOG(u8"[提示词] 为角色 " << name << u8" 生成中...\n");
    std::string prompt = g_turn_callDeepSeek("deepseek-v4-flash", messages);
    prompt = CleanUTF8(prompt);
    DEBUG_LOG(u8"[提示词] 结果: " << prompt << u8"\n");
    res.set_content(nlohmann::json({ {"prompt", prompt} }).dump(), "application/json");
}

static void HandleSetCharacterAvatar(const httplib::Request& req, httplib::Response& res) {
    auto j = nlohmann::json::parse(req.body);
    std::string characterId = j["character_id"];
    std::string avatarUrl = j["avatar_url"];
    if (characterId == "protagonist") {
        g_turn_ctx->protagonistAvatar = avatarUrl;
        g_turn_ctx->SaveStoryData(g_turn_dataDir);
        res.set_content(u8"{\"success\":true}", "application/json");
        return;
    }
    std::string hashId = characterId;
    if (g_turn_ctx->characterProfiles.find(hashId) != g_turn_ctx->characterProfiles.end()) {
        g_turn_ctx->characterProfiles[hashId]["avatar_url"] = avatarUrl;
        g_turn_ctx->SaveStoryData(g_turn_dataDir);
    }
    res.set_content(u8"{\"success\":true}", "application/json");
}

void RegisterTurnRoutes(httplib::Server& svr, const std::string& dataDir, StoryContext& ctx,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek) {
    g_turn_ctx = &ctx;
    g_turn_dataDir = dataDir;
    g_turn_callDeepSeek = callDeepSeek;
    svr.Post("/api/turn", HandleTurn);
    svr.Post("/api/generate_avatar_prompt", HandleGenerateAvatarPrompt);
    svr.Post("/api/set_character_avatar", HandleSetCharacterAvatar);
    svr.Get("/api/character_info", HandleGetCharacterInfo);
}