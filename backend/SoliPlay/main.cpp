// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <httplib.h>
#include <iostream>
#include <string>
#include <windows.h>
#include <thread>
#include <chrono>
#include <direct.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <shellapi.h>
#include <nlohmann/json.hpp>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#include "debug.h"
#include "utils.h"
#include "sse_handler.h"
#include "story_manager.h"
#include "api_handler.h"
#include "logger.h"

#define SOLIPLAY_VERSION "v0.2.0"

// 声明其他翻译单元的全局变量
extern StoryContext* g_turn_ctx;
// 声明角色资料卡生成用的全局 LLM 函数指针（定义在 story_manager.cpp）
extern std::function<std::string(const std::string&, const nlohmann::json&)> g_card_callDeepSeek;

static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string FindProjectRoot() {
    std::string exeDir = GetExeDir();
    std::string current = exeDir;
    for (int i = 0; i < 10; ++i) {
        std::string testPath = current + "\\frontend";
        DWORD attrib = GetFileAttributesA(testPath.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return current;
        }
        size_t pos = current.find_last_of("\\/");
        if (pos == std::string::npos) break;
        current = current.substr(0, pos);
        if (current.empty() || current.size() <= 2) break;
    }
    return exeDir;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    std::string rootDir = FindProjectRoot();
    std::string frontendDir = rootDir + "\\frontend";
    std::string dataDir = rootDir + "\\data";
    std::string avatarDir = frontendDir + "\\asset\\profile";
    std::string logDir = rootDir + "\\backend\\logs";
    _mkdir(dataDir.c_str());
    _mkdir(avatarDir.c_str());
    _mkdir(logDir.c_str());

    // 检查数据目录可写性
    std::string testFile = dataDir + "\\write_test.tmp";
    std::ofstream testOut(testFile);
    if (!testOut.is_open()) {
        MessageBoxA(NULL, "程序数据目录没有写入权限！\n\n请将SoliPlay整个文件夹移动到非系统保护目录（如桌面或D盘），然后重新启动。", "权限错误", MB_ICONERROR);
        return 1;
    }
    testOut.close();
    remove(testFile.c_str());

    Logger::Instance().Init(logDir);
    Debug::CoreInfo(u8"服务器启动中...");

    std::string configPath = dataDir + "\\config.json";
    std::string apiKey;
    int debugMode = 0;
    bool llmLogToStory = false;

    const char* envKey = std::getenv("DEEPSEEK_API_KEY");
    if (envKey) apiKey = Trim(envKey);

    if (apiKey.empty()) {
        std::ifstream configFile(configPath);
        if (configFile.is_open()) {
            nlohmann::json config;
            configFile >> config;
            configFile.close();
            debugMode = config.value("debug", 0);
            llmLogToStory = config.value("llm_log_to_story", false);
            std::string fileKey = config.value("api_key", "");
            if (!fileKey.empty()) {
                std::string trimmedKey = Trim(fileKey);
                if (!trimmedKey.empty()) {
                    if (SetPermanentEnvVar("DEEPSEEK_API_KEY", trimmedKey)) {
                        apiKey = trimmedKey;
                        config.erase("api_key");
                        std::ofstream out(configPath);
                        out << config.dump(2);
                        out.close();
                    }
                }
            }
        }
    }
    else {
        std::ifstream configFile(configPath);
        if (configFile.is_open()) {
            nlohmann::json config;
            configFile >> config;
            configFile.close();
            debugMode = config.value("debug", 0);
            llmLogToStory = config.value("llm_log_to_story", false);
        }
    }

    if (debugMode == 1 || std::getenv("SOLIPLAY_DEBUG")) {
        Debug::enabled = true;
        DEBUG_LOG(u8"[调试] 调试输出已开启" << std::endl);
    }

    Logger::Instance().SetLLMLogEnabled(llmLogToStory);

    auto callDeepSeek = [&](const std::string& model, const nlohmann::json& messages) -> std::string {
        if (apiKey.empty()) {
            std::cerr << u8"[LLM] 错误：未设置 API Key，请在前端设置" << std::endl;
            Debug::CoreError(u8"LLM调用失败：未设置 API Key");
            return "";
        }
        DEBUG_LOG(u8"[LLM] 调用模型: " << model << u8", 消息数量: " << messages.size() << std::endl);
        if (Debug::enabled) {
            DEBUG_LOG(u8"[LLM] 请求体:\n" << messages.dump(2) << u8"\n");
        }

        nlohmann::json cleanMessages = nlohmann::json::array();
        for (const auto& msg : messages) {
            if (msg.is_object() && msg.contains("role") && msg.contains("content")) {
                cleanMessages.push_back(msg);
            }
            else if (msg.is_array()) {
                nlohmann::json obj;
                for (const auto& element : msg) {
                    if (element.is_array() && element.size() == 2) {
                        obj[element.at(0).get<std::string>()] = element.at(1).get<std::string>();
                    }
                }
                if (obj.contains("role") && obj.contains("content")) {
                    cleanMessages.push_back(obj);
                }
                else {
                    DEBUG_LOG(u8"[LLM] 丢弃无效数组消息: " << msg.dump() << std::endl);
                }
            }
            else {
                DEBUG_LOG(u8"[LLM] 丢弃无效消息: " << msg.dump() << std::endl);
            }
        }

        httplib::Client cli("https://api.deepseek.com");
        cli.set_connection_timeout(30);
        cli.set_read_timeout(30);
        nlohmann::json body;
        body["model"] = model;
        body["messages"] = cleanMessages;
        body["stream"] = false;

        bool needJson = g_turn_ctx ? g_turn_ctx->nextCallJsonMode : false;
        if (needJson) {
            body["response_format"] = { {"type", "json_object"} };
        }

        auto res = cli.Post("/chat/completions",
            { {"Authorization", "Bearer " + apiKey} },
            body.dump(), "application/json");
        if (!res) {
            Debug::CoreError(u8"LLM请求失败: " + httplib::to_string(res.error()));
            std::cerr << u8"[LLM] 请求失败: " << httplib::to_string(res.error()) << std::endl;
            return "";
        }
        if (res->status != 200) {
            Debug::CoreError(u8"LLM HTTP错误 " + std::to_string(res->status) + ": " + res->body);
            std::cerr << u8"[LLM] HTTP错误 " << res->status << ": " << res->body << std::endl;
            return "";
        }
        auto j = nlohmann::json::parse(res->body);
        std::string reply = j["choices"][0]["message"]["content"];
        reply = CleanUTF8(reply);
        DEBUG_LOG(u8"[LLM] 收到回复，长度: " << reply.size() << std::endl);
        if (Debug::enabled) {
            DEBUG_LOG(u8"[LLM] 回复内容:\n" << reply << u8"\n");
        }

        if (g_turn_ctx && !g_turn_ctx->currentStoryId.empty()) {
            std::string logContent = "Request:\n" + body.dump(2) + "\n\nResponse:\n" + reply;
            Logger::Instance().LLMLog(g_turn_ctx->currentStoryId, logContent);
        }

        return reply;
        };

    // 设置角色资料卡生成用的全局 LLM 函数
    g_card_callDeepSeek = callDeepSeek;

    Debug::CoreInfo(u8"API Key 状态: " + (apiKey.empty() ? std::string(u8"未设置") : std::string(u8"已设置")));

    DEBUG_LOG(u8"[初始化] 项目根目录: " << rootDir << std::endl);
    DEBUG_LOG(u8"[初始化] 前端目录: " << frontendDir << std::endl);
    DEBUG_LOG(u8"[初始化] 数据目录: " << dataDir << std::endl);
    DEBUG_LOG(u8"[初始化] 头像目录: " << avatarDir << std::endl);

    httplib::Server svr;
    svr.set_mount_point("/", frontendDir);

    svr.Get("/api/config", [&apiKey, &llmLogToStory](const httplib::Request&, httplib::Response& res) {
        nlohmann::json config;
        config["debug"] = Debug::enabled ? 1 : 0;
        config["retry_count"] = 3;
        config["auto_save"] = true;
        config["has_api_key"] = !apiKey.empty();
        config["llm_log_to_story"] = llmLogToStory;
        config["version"] = SOLIPLAY_VERSION;
        res.set_content(config.dump(), "application/json");
        });

    svr.Post("/api/save_config", [&apiKey, &dataDir, &llmLogToStory](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            if (j.contains("api_key")) {
                std::string newKey = Trim(j["api_key"].get<std::string>());
                if (!newKey.empty()) {
                    if (!SetPermanentEnvVar("DEEPSEEK_API_KEY", newKey)) {
                        res.status = 500;
                        res.set_content(u8"{\"error\":\"无法设置环境变量\"}", "application/json");
                        return;
                    }
                    apiKey = newKey;
                }
            }
            if (j.contains("debug")) Debug::enabled = (j["debug"].get<int>() != 0);
            if (j.contains("llm_log_to_story")) {
                llmLogToStory = j["llm_log_to_story"].get<bool>();
                Logger::Instance().SetLLMLogEnabled(llmLogToStory);
            }
            nlohmann::json config;
            config["debug"] = Debug::enabled ? 1 : 0;
            config["llm_log_to_story"] = llmLogToStory;
            std::ofstream o(dataDir + "\\config.json");
            o << config.dump(2);
            o.close();
            res.set_content(u8"{\"success\":true}", "application/json");
        }
        catch (...) {
            std::cerr << u8"[设置] 无效请求" << std::endl;
            res.status = 400;
            res.set_content(u8"{\"error\":\"无效请求\"}", "application/json");
        }
        });

    // 自动更新 API
    svr.Post("/api/update", [&](const httplib::Request& req, httplib::Response& res) {
        std::vector<std::string> mirrors = {
            "https://gitee.com/Coore309/SoliPlay/releases/download/" SOLIPLAY_VERSION "/SoliPlay.zip",
            "https://github.com/Coore309/SoliPlay/releases/download/" SOLIPLAY_VERSION "/SoliPlay.zip"
        };
        std::string tempZip = rootDir + "\\SoliPlay_update.zip";
        bool downloaded = false;
        for (const auto& url : mirrors) {
            if (URLDownloadToFileA(NULL, url.c_str(), tempZip.c_str(), 0, NULL) == S_OK) {
                downloaded = true;
                break;
            }
        }
        if (!downloaded) {
            res.set_content(u8"{\"error\":\"下载更新失败\"}", "application/json");
            return;
        }
        std::string updaterPath = rootDir + "\\updater.exe";
        std::string cmdLine = "\"" + updaterPath + "\" \"" + tempZip + "\"";
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else {
            res.set_content(u8"{\"error\":\"启动更新器失败\"}", "application/json");
            return;
        }
        res.set_content(u8"{\"success\":true, \"message\":\"更新已开始，程序即将重启\"}", "application/json");
        std::thread([]() {
            Sleep(1000);
            exit(0);
            }).detach();
        });

    svr.Post("/api/quit", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
        exit(0);
        });

    StoryContext ctx;

    RegisterStreamRoutes(svr, ctx);
    RegisterStoryRoutes(svr, dataDir, ctx, callDeepSeek);
    RegisterAvatarRoutes(svr, avatarDir, ctx, callDeepSeek, dataDir);
    RegisterTurnRoutes(svr, dataDir, ctx, callDeepSeek);

    DEBUG_LOG(u8"[服务器] 路由设置完成，正在启动..." << std::endl);
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "\n========================================" << std::endl;
        std::cout << u8"  SoliPlay 服务器启动成功！" << std::endl;
        std::cout << u8"  http://localhost:8080" << std::endl;
        std::cout << "========================================\n" << std::endl;
        ShellExecuteA(NULL, "open", "http://localhost:8080", NULL, NULL, SW_SHOWNORMAL);
        }).detach();

    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << u8"[服务器] 监听端口 8080 失败！" << std::endl;
        system("pause");
        return 1;
    }

    DEBUG_LOG(u8"[服务器] 已关闭。" << std::endl);
    return 0;
}