// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "api_handler.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <direct.h>
#include "utils.h"
#include "debug.h"

static std::string g_avatarDir;
static StoryContext* g_avatar_ctx = nullptr;
static std::string g_avatar_dataDir;
static std::function<std::string(const std::string&, const nlohmann::json&)> g_avatar_callDeepSeek;

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static inline bool is_base64(unsigned char c) { return (isalnum(c) || (c == '+') || (c == '/')); }
static std::string base64_decode(const std::string& encoded_string) {
    size_t in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; i < 3; i++) ret += char_array_3[i];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        for (j = 0; j < 4; j++) char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; j < i - 1; j++) ret += char_array_3[j];
    }
    return ret;
}

static void HandleGetAvatarFolders(const httplib::Request&, httplib::Response& res) {
    nlohmann::json list = nlohmann::json::array();
    auto dirs = ListSubdirectories(g_avatarDir);
    for (const auto& dir : dirs) list.push_back(dir);
    res.set_content(list.dump(), "application/json");
}

static void HandleCreateAvatarFolder(const httplib::Request& req, httplib::Response& res) {
    auto j = nlohmann::json::parse(req.body);
    std::string folderName = j["folder_name"];
    std::string folderPath = g_avatarDir + "\\" + UTF8ToACP(folderName);
    _mkdir(folderPath.c_str());
    res.set_content(u8"{\"success\":true}", "application/json");
}

static void HandleGetAvatars(const httplib::Request& req, httplib::Response& res) {
    std::string folder = req.get_param_value("folder");
    std::string searchDir = g_avatarDir;
    if (!folder.empty()) searchDir += "\\" + UTF8ToACP(folder);
    nlohmann::json list = nlohmann::json::array();
    std::string searchPath = searchDir + "\\*.*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string fname = findData.cFileName;
                std::string fnameUtf8 = ACPToUTF8(fname);
                if (fnameUtf8.size() > 4 && (fnameUtf8.substr(fnameUtf8.size() - 4) == ".png" || fnameUtf8.substr(fnameUtf8.size() - 4) == ".jpg" || fnameUtf8.substr(fnameUtf8.size() - 5) == ".jpeg")) {
                    std::string url = "/asset/profile/";
                    if (!folder.empty()) url += folder + "/";
                    url += fnameUtf8;
                    list.push_back(url);
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    res.set_content(list.dump(), "application/json");
}

static void HandleUploadAvatar(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = nlohmann::json::parse(req.body);
        std::string dataUrl = j["data_url"];
        std::string folder = j.value("folder", "");
        std::string prefix = "base64,";
        size_t pos = dataUrl.find(prefix);
        if (pos == std::string::npos) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"无效的图片数据\"}", "application/json");
            return;
        }
        std::string base64 = dataUrl.substr(pos + prefix.length());
        std::string decoded = base64_decode(base64);
        std::string filename = "avatar_" + GenerateHash() + ".png";
        std::string saveDir = g_avatarDir;
        if (!folder.empty()) saveDir += "\\" + UTF8ToACP(folder);
        _mkdir(saveDir.c_str());
        std::string filepath = saveDir + "\\" + filename;
        std::ofstream out(filepath, std::ios::binary);
        out.write(decoded.c_str(), decoded.size());
        out.close();
        nlohmann::json result;
        std::string url = "/asset/profile/";
        if (!folder.empty()) url += folder + "/";
        url += filename;
        result["url"] = url;
        res.set_content(result.dump(), "application/json");
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"上传失败\"}", "application/json");
    }
}

static void HandleDeleteAvatar(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = nlohmann::json::parse(req.body);
        std::string filename = j["filename"];
        std::string folder = j.value("folder", "");
        std::string filepath = g_avatarDir;
        if (!folder.empty()) filepath += "\\" + UTF8ToACP(folder);
        filepath += "\\" + UTF8ToACP(filename);
        if (remove(filepath.c_str()) == 0) {
            res.set_content(u8"{\"success\":true}", "application/json");
        }
        else {
            res.status = 404;
            res.set_content(u8"{\"error\":\"文件不存在\"}", "application/json");
        }
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"删除失败\"}", "application/json");
    }
}

// 删除文件夹（仅空文件夹可删）
static void HandleDeleteAvatarFolder(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = nlohmann::json::parse(req.body);
        std::string folder = j.value("folder", "");
        if (folder.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"文件夹名不能为空\"}", "application/json");
            return;
        }
        std::string folderPath = g_avatarDir + "\\" + UTF8ToACP(folder);
        // 检查文件夹是否为空
        std::string searchPath = folderPath + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            res.status = 404;
            res.set_content(u8"{\"error\":\"文件夹不存在\"}", "application/json");
            return;
        }
        bool isEmpty = true;
        do {
            if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
                isEmpty = false;
                break;
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
        if (!isEmpty) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"文件夹不为空，无法删除\"}", "application/json");
            return;
        }
        if (RemoveDirectoryA(folderPath.c_str())) {
            res.set_content(u8"{\"success\":true}", "application/json");
        }
        else {
            res.status = 500;
            res.set_content(u8"{\"error\":\"删除失败\"}", "application/json");
        }
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"删除文件夹异常\"}", "application/json");
    }
}

// 移动头像文件到指定文件夹
static void HandleMoveAvatar(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = nlohmann::json::parse(req.body);
        std::string filename = j["filename"];           // UTF-8
        std::string sourceFolder = j.value("source_folder", "");
        std::string targetFolder = j.value("target_folder", "");

        if (filename.empty()) {
            res.status = 400;
            res.set_content(u8"{\"error\":\"文件名不能为空\"}", "application/json");
            return;
        }

        std::string sourcePath = g_avatarDir;
        if (!sourceFolder.empty()) sourcePath += "\\" + UTF8ToACP(sourceFolder);
        sourcePath += "\\" + UTF8ToACP(filename);

        std::string targetDir = g_avatarDir;
        if (!targetFolder.empty()) targetDir += "\\" + UTF8ToACP(targetFolder);
        _mkdir(targetDir.c_str());
        std::string targetPath = targetDir + "\\" + UTF8ToACP(filename);

        if (MoveFileA(sourcePath.c_str(), targetPath.c_str())) {
            res.set_content(u8"{\"success\":true}", "application/json");
        }
        else {
            res.status = 500;
            res.set_content(u8"{\"error\":\"移动失败\"}", "application/json");
        }
    }
    catch (...) {
        res.status = 500;
        res.set_content(u8"{\"error\":\"移动异常\"}", "application/json");
    }
}

void RegisterAvatarRoutes(httplib::Server& svr, const std::string& avatarDir, StoryContext& ctx,
    std::function<std::string(const std::string&, const nlohmann::json&)> callDeepSeek,
    const std::string& dataDir) {
    g_avatarDir = avatarDir;
    g_avatar_ctx = &ctx;
    g_avatar_dataDir = dataDir;
    g_avatar_callDeepSeek = callDeepSeek;
    svr.Get("/api/avatar_folders", HandleGetAvatarFolders);
    svr.Post("/api/create_avatar_folder", HandleCreateAvatarFolder);
    svr.Get("/api/avatars", HandleGetAvatars);
    svr.Post("/api/upload_avatar", HandleUploadAvatar);
    svr.Post("/api/delete_avatar", HandleDeleteAvatar);
    svr.Post("/api/delete_avatar_folder", HandleDeleteAvatarFolder);
    svr.Post("/api/move_avatar", HandleMoveAvatar);
}