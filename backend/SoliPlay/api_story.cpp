// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "api_handler.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <direct.h>
#include <shellapi.h>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>
#include "utils.h"
#include "debug.h"

static std::string g_stories_dataDir;
static StoryContext* g_stories_ctx = nullptr;
static std::function<std::string(const std::string&, const nlohmann::json&)> g_stories_callDeepSeek;

static void HandleGetStories(const httplib::Request&, httplib::Response& res) {
    DEBUG_LOG(u8"[API] 请求故事列表\n");
    nlohmann::json storyList = nlohmann::json::array();
    std::string storiesDir = g_stories_dataDir + "\\stories";
    _mkdir(storiesDir.c_str());
    auto dirs = ListSubdirectories(storiesDir);
    for (const auto& storyId : dirs) {
        std::ifstream metaFile(storiesDir + "\\" + storyId + "\\meta.json");
        if (metaFile.is_open()) {
            nlohmann::json meta;
            metaFile >> meta;
            meta["id"] = storyId;
            storyList.push_back(meta);
            metaFile.close();
        }
    }
    std::sort(storyList.begin(), storyList.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
        return a.value("last_played", 0) > b.value("last_played", 0);
        });
    res.set_content(storyList.dump(), "application/json");
}

static void HandleLoadStory(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string storyId = reqJson.value("story_id", "");
        if (storyId.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"缺少 story_id\"}", "application/json");
            return;
        }
        if (!g_stories_ctx->LoadStoryData(g_stories_dataDir, storyId)) {
            res.status = 404;
            res.set_content(u8"{\"error\":\"故事不存在\"}", "application/json");
            return;
        }
        nlohmann::json response;
        response["success"] = true;
        response["chat_messages"] = g_stories_ctx->chatMessages;
        response["story_id"] = g_stories_ctx->currentStoryId;
        res.set_content(response.dump(), "application/json");
    }
    catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"加载失败\"}", "application/json");
    }
}

static void HandleDeleteStory(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string storyId = reqJson.value("story_id", "");
        if (storyId.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"缺少 story_id\"}", "application/json");
            return;
        }
        std::string storyDir = g_stories_dataDir + "\\stories\\" + storyId;
        std::string cmd = "rmdir /s /q \"" + storyDir + "\"";
        system(cmd.c_str());
        res.set_content(u8"{\"success\":true}", "application/json");
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"删除失败\"}", "application/json");
    }
}

static void HandleRenameStory(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string storyId = reqJson.value("story_id", "");
        std::string newTitle = reqJson.value("title", "");
        if (storyId.empty() || newTitle.empty()) {
            res.status = 400;
            return;
        }
        std::string metaPath = g_stories_dataDir + "\\stories\\" + storyId + "\\meta.json";
        nlohmann::json meta;
        std::ifstream in(metaPath);
        if (in.is_open()) {
            in >> meta;
            in.close();
            meta["title"] = newTitle;
            std::ofstream out(metaPath);
            out << meta.dump(2);
            out.close();
            res.set_content(u8"{\"success\":true}", "application/json");
        }
        else {
            res.status = 404;
            res.set_content(u8"{\"error\":\"故事不存在\"}", "application/json");
        }
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"重命名失败\"}", "application/json");
    }
}

static void HandleOpenStoryFolder(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string storyId = reqJson.value("story_id", "");
        if (storyId.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"缺少 story_id\"}", "application/json");
            return;
        }
        std::string folderPath = g_stories_dataDir + "\\stories\\" + storyId;
        DWORD attrib = GetFileAttributesA(folderPath.c_str());
        if (attrib == INVALID_FILE_ATTRIBUTES || !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            res.status = 404;
            res.set_content(u8"{\"error\":\"文件夹不存在\"}", "application/json");
            return;
        }
        ShellExecuteA(NULL, "open", folderPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        res.set_content(u8"{\"success\":true}", "application/json");
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"打开失败\"}", "application/json");
    }
}

static void HandleExportStory(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string storyId = reqJson.value("story_id", "");
        if (storyId.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"缺少 story_id\"}", "application/json");
            return;
        }
        if (g_stories_ctx->currentStoryId != storyId) {
            if (!g_stories_ctx->LoadStoryData(g_stories_dataDir, storyId)) {
                res.status = 404;
                res.set_content(u8"{\"error\":\"故事不存在\"}", "application/json");
                return;
            }
        }
        std::ostringstream oss;
        for (const auto& msg : g_stories_ctx->chatMessages) {
            std::string type = msg.value("type", "");
            if (type == "character_dialogue") {
                std::string characterId = msg.value("character_id", "");
                std::string text = msg.value("text", "");
                std::string trueName;
                if (characterId == "protagonist") {
                    trueName = g_stories_ctx->protagonistData.value("name", u8"你");
                }
                else {
                    auto it = g_stories_ctx->characterProfiles.find(characterId);
                    if (it != g_stories_ctx->characterProfiles.end()) {
                        trueName = it->second.value("true_name", u8"未知");
                    }
                    else {
                        trueName = msg.value("name", u8"未知");
                    }
                }
                oss << trueName << u8"：" << text << "\n";
            }
            else if (type == "narrator") {
                std::string text = msg.value("text", "");
                if (!text.empty()) {
                    oss << text << "\n";
                }
            }
        }
        std::string exportContent = oss.str();
        std::string storyDir = g_stories_dataDir + "\\stories\\" + storyId;
        _mkdir(storyDir.c_str());
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &t);
        std::ostringstream filename;
        filename << "story_export_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".txt";
        std::string filePath = storyDir + "\\" + filename.str();
        std::ofstream out(filePath, std::ios::out | std::ios::binary);
        if (!out.is_open()) {
            res.status = 500;
            res.set_content(u8"{\"error\":\"无法创建导出文件\"}", "application/json");
            return;
        }
        out << exportContent;
        out.close();
        res.set_header("Content-Type", "text/plain; charset=utf-8");
        res.set_header("Content-Disposition", "attachment; filename=\"" + filename.str() + "\"");
        res.set_content(exportContent, "text/plain; charset=utf-8");
    }
    catch (const std::exception& e) {
        std::cerr << u8"[导出] 异常: " << e.what() << std::endl;
        res.status = 500;
        res.set_content(u8"{\"error\":\"导出失败\"}", "application/json");
    }
}

static void HandleRescueStory(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(req.body);
        std::string storyId = reqJson.value("story_id", "");
        if (storyId.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"缺少 story_id\"}", "application/json");
            return;
        }

        if (!g_stories_ctx->LoadStoryData(g_stories_dataDir, storyId)) {
            res.status = 404;
            res.set_content(u8"{\"error\":\"故事不存在\"}", "application/json");
            return;
        }

        g_stories_ctx->directorHistory.clear();
        g_stories_ctx->characterHistories.clear();

        std::string rescueSystemPrompt = u8R"(你是 SoliPlay 的游戏导演。你的输出必须是严格的 JSON 对象，使用以下字段：
- "narration": 旁白文本
- "dialogues": 数组，每个元素为 {"character_id": "...", "name": "...", "text": "...", "avatar_url": "..."}
- "input_prompt": 提示玩家输入的文本
故事已经进行了一段时间，但导演记忆丢失。请根据下面提供的世界观、角色档案和最近的聊天记录，生成一段衔接的旁白和对话，让故事自然地继续下去。不要重新开始，要无缝衔接。)";
        g_stories_ctx->directorHistory.push_back({ {"role", "system"}, {"content", rescueSystemPrompt} });

        nlohmann::json rescueInfo;
        rescueInfo["world_overview"] = g_stories_ctx->worldOverview;
        rescueInfo["protagonist"] = g_stories_ctx->protagonistData;
        nlohmann::json charsBrief = nlohmann::json::array();
        for (auto& pair : g_stories_ctx->characterProfiles) {
            auto& p = pair.second;
            nlohmann::json brief;
            brief["character_id"] = p.value("original_id", "");
            brief["true_name"] = p.value("true_name", "");
            brief["display_name"] = p.value("display_name", "");
            brief["background"] = p.value("background", "");
            brief["personality"] = p.value("personality", "");
            brief["attitude"] = p.value("initial_attitude_to_player", "");
            charsBrief.push_back(brief);
        }
        rescueInfo["characters"] = charsBrief;
        if (g_stories_ctx->chatMessages.is_array() && !g_stories_ctx->chatMessages.empty()) {
            int start = std::max(0, (int)g_stories_ctx->chatMessages.size() - 5);
            nlohmann::json recent = nlohmann::json::array();
            for (int i = start; i < (int)g_stories_ctx->chatMessages.size(); ++i) {
                recent.push_back(g_stories_ctx->chatMessages[i]);
            }
            rescueInfo["recent_chat"] = recent;
        }
        std::string rescueContent = rescueInfo.dump();

        g_stories_ctx->directorHistory.push_back({ {"role", "user"}, {"content", u8"请生成衔接剧情（JSON）：\n" + rescueContent} });

        g_stories_ctx->nextCallJsonMode = true;
        std::string directorReply = g_stories_callDeepSeek("deepseek-v4-pro", g_stories_ctx->directorHistory);
        g_stories_ctx->nextCallJsonMode = false;

        if (!directorReply.empty()) {
            directorReply = CleanUTF8(directorReply);
            nlohmann::json directorJson;
            bool parsed = false;
            try {
                directorJson = nlohmann::json::parse(directorReply);
                parsed = true;
            }
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
                try {
                    directorJson = nlohmann::json::parse(clean);
                    parsed = true;
                }
                catch (...) {}
            }

            if (parsed) {
                std::string narration = directorJson.value("narration", directorJson.value("narrative", ""));
                if (!narration.empty()) {
                    nlohmann::json narrMsg;
                    narrMsg["type"] = "narrator";
                    narrMsg["text"] = narration;
                    g_stories_ctx->chatMessages.push_back(narrMsg);
                }
                if (directorJson.contains("dialogues") && directorJson["dialogues"].is_array()) {
                    for (auto& d : directorJson["dialogues"]) {
                        if (d.is_object() && d.contains("text")) {
                            nlohmann::json charMsg;
                            charMsg["type"] = "character_dialogue";
                            charMsg["character_id"] = d.value("character_id", "unknown");
                            charMsg["name"] = d.value("name", d.value("display_name", "某人"));
                            charMsg["text"] = d.value("text", "");
                            charMsg["avatar_url"] = d.value("avatar_url", "/asset/profile/default.png");
                            g_stories_ctx->chatMessages.push_back(charMsg);
                        }
                    }
                }
                std::string inputPrompt = directorJson.value("input_prompt", u8"请输入你的行动或对话...");
                nlohmann::json promptMsg;
                promptMsg["type"] = "input_required";
                promptMsg["prompt"] = inputPrompt;
                g_stories_ctx->chatMessages.push_back(promptMsg);
            }
            else {
                std::string fallbackNarration = directorReply;
                if (g_stories_ctx->chatMessages.is_array() && !g_stories_ctx->chatMessages.empty()) {
                    const auto& lastMsg = g_stories_ctx->chatMessages.back();
                    if (lastMsg.contains("text") && lastMsg["text"].is_string()) {
                        std::string lastText = lastMsg["text"].get<std::string>();
                        fallbackNarration += u8"\n\n（最近发生的事情：）" + lastText;
                    }
                }
                nlohmann::json narrMsg;
                narrMsg["type"] = "narrator";
                narrMsg["text"] = fallbackNarration;
                g_stories_ctx->chatMessages.push_back(narrMsg);
                nlohmann::json promptMsg;
                promptMsg["type"] = "input_required";
                promptMsg["prompt"] = u8"请输入你的行动或对话...";
                g_stories_ctx->chatMessages.push_back(promptMsg);
            }
        }
        else {
            std::string fallbackNarration = u8"故事已重新衔接。";
            if (g_stories_ctx->chatMessages.is_array() && !g_stories_ctx->chatMessages.empty()) {
                const auto& lastMsg = g_stories_ctx->chatMessages.back();
                if (lastMsg.contains("text") && lastMsg["text"].is_string()) {
                    std::string lastText = lastMsg["text"].get<std::string>();
                    fallbackNarration += u8"\n你想起之前的事：" + lastText + u8"\n现在你决定继续前进。";
                }
            }
            nlohmann::json narrMsg;
            narrMsg["type"] = "narrator";
            narrMsg["text"] = fallbackNarration;
            g_stories_ctx->chatMessages.push_back(narrMsg);
            nlohmann::json promptMsg;
            promptMsg["type"] = "input_required";
            promptMsg["prompt"] = u8"请输入你的行动或对话...";
            g_stories_ctx->chatMessages.push_back(promptMsg);
        }

        g_stories_ctx->SaveStoryData(g_stories_dataDir);

        nlohmann::json response;
        response["success"] = true;
        res.set_content(response.dump(), "application/json");
    }
    catch (const std::exception& e) {
        std::cerr << u8"[抢救] 严重异常: " << e.what() << std::endl;
        try {
            nlohmann::json narrMsg;
            narrMsg["type"] = "narrator";
            narrMsg["text"] = u8"故事抢救过程中出现技术问题，但你的进度没有丢失。请继续冒险。";
            g_stories_ctx->chatMessages.push_back(narrMsg);
            nlohmann::json promptMsg;
            promptMsg["type"] = "input_required";
            promptMsg["prompt"] = u8"请输入你的行动或对话...";
            g_stories_ctx->chatMessages.push_back(promptMsg);
            g_stories_ctx->SaveStoryData(g_stories_dataDir);
            nlohmann::json response;
            response["success"] = true;
            res.set_content(response.dump(), "application/json");
        }
        catch (...) {
            res.status = 500;
            res.set_content(u8"{\"error\":\"抢救失败，但存档文件未受损。请重新打开故事或联系支持。\"}", "application/json");
        }
    }
}

void RegisterStoryRoutes(httplib::Server& svr, const std::string& dataDir, StoryContext& ctx,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek) {
    g_stories_dataDir = dataDir;
    g_stories_ctx = &ctx;
    g_stories_callDeepSeek = callDeepSeek;
    svr.Get("/api/stories", HandleGetStories);
    svr.Post("/api/load_story", HandleLoadStory);
    svr.Post("/api/delete_story", HandleDeleteStory);
    svr.Post("/api/rename_story", HandleRenameStory);
    svr.Post("/api/open_story_folder", HandleOpenStoryFolder);
    svr.Post("/api/rescue_story", HandleRescueStory);
    svr.Post("/api/export_story", HandleExportStory);
}