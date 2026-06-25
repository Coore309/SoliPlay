// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "utils.h"
#include "logger.h"          // 添加，用于日志记录
#include <windows.h>
#include <random>
#include <chrono>
#include <sstream>
#include <iostream>

std::string UTF8ToACP(const std::string& utf8) {
    if (utf8.empty()) return "";
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (wideLen <= 0) return utf8;
    std::wstring wide(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], wideLen);
    int acpLen = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), NULL, 0, NULL, NULL);
    if (acpLen <= 0) return utf8;
    std::string acp(acpLen, 0);
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), &acp[0], acpLen, NULL, NULL);
    return acp;
}

std::string ACPToUTF8(const std::string& acp) {
    if (acp.empty()) return "";
    int wideLen = MultiByteToWideChar(CP_ACP, 0, acp.c_str(), (int)acp.size(), NULL, 0);
    if (wideLen <= 0) return acp;
    std::wstring wide(wideLen, 0);
    MultiByteToWideChar(CP_ACP, 0, acp.c_str(), (int)acp.size(), &wide[0], wideLen);
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), NULL, 0, NULL, NULL);
    if (utf8Len <= 0) return acp;
    std::string utf8(utf8Len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &utf8[0], utf8Len, NULL, NULL);
    return utf8;
}

std::string GetExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    return fullPath.substr(0, pos);
}

std::string GenerateHash() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::mt19937 rng(static_cast<unsigned>(ms));
    std::uniform_int_distribution<int> dist(1000, 9999);
    std::stringstream ss;
    ss << std::hex << ms << "_" << dist(rng);
    return ss.str();
}

std::vector<std::string> ListSubdirectories(const std::string& path) {
    std::vector<std::string> result;
    std::string searchPath = path + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return result;
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string name = findData.cFileName;
            if (name != "." && name != "..") {
                result.push_back(ACPToUTF8(name));
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
    return result;
}

std::string CleanUTF8(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    size_t i = 0;
    static int badCount = 0;
    while (i < input.size()) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c <= 0x7F) {
            output.push_back(c);
            ++i;
        }
        else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 < input.size() && (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80) {
                output.push_back(c);
                output.push_back(input[i + 1]);
                i += 2;
            }
            else {
                if (badCount < 5) {
                    Logger::Instance().Error("CleanUTF8: invalid 2-byte sequence at " + std::to_string(i));
                }
                badCount++;
                output.push_back('?'); ++i;
            }
        }
        else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 < input.size() &&
                (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(input[i + 2]) & 0xC0) == 0x80) {
                if (c == 0xE0 && (unsigned char)(input[i + 1]) < 0xA0) {
                    if (badCount < 5) Logger::Instance().Error("CleanUTF8: overlong 3-byte at " + std::to_string(i));
                    badCount++;
                    output.push_back('?'); ++i;
                }
                else if (c == 0xED && (unsigned char)(input[i + 1]) > 0x9F) {
                    if (badCount < 5) Logger::Instance().Error("CleanUTF8: surrogate 3-byte at " + std::to_string(i));
                    badCount++;
                    output.push_back('?'); ++i;
                }
                else {
                    output.push_back(c);
                    output.push_back(input[i + 1]);
                    output.push_back(input[i + 2]);
                    i += 3;
                }
            }
            else {
                if (badCount < 5) Logger::Instance().Error("CleanUTF8: invalid 3-byte sequence at " + std::to_string(i));
                badCount++;
                output.push_back('?'); ++i;
            }
        }
        else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 < input.size() &&
                (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(input[i + 2]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(input[i + 3]) & 0xC0) == 0x80) {
                if (c == 0xF0 && (unsigned char)(input[i + 1]) < 0x90) {
                    if (badCount < 5) Logger::Instance().Error("CleanUTF8: overlong 4-byte at " + std::to_string(i));
                    badCount++;
                    output.push_back('?'); ++i;
                }
                else if (c == 0xF4 && (unsigned char)(input[i + 1]) > 0x8F) {
                    if (badCount < 5) Logger::Instance().Error("CleanUTF8: out-of-range 4-byte at " + std::to_string(i));
                    badCount++;
                    output.push_back('?'); ++i;
                }
                else {
                    output.push_back(c);
                    output.push_back(input[i + 1]);
                    output.push_back(input[i + 2]);
                    output.push_back(input[i + 3]);
                    i += 4;
                }
            }
            else {
                if (badCount < 5) Logger::Instance().Error("CleanUTF8: invalid 4-byte sequence at " + std::to_string(i));
                badCount++;
                output.push_back('?'); ++i;
            }
        }
        else {
            if (badCount < 5) Logger::Instance().Error("CleanUTF8: invalid start byte at " + std::to_string(i));
            badCount++;
            output.push_back('?'); ++i;
        }
    }
    return output;
}

bool SetPermanentEnvVar(const std::string& name, const std::string& value) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_WRITE | KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return false;
    LONG result;
    if (value.empty()) {
        result = RegDeleteValueA(hKey, name.c_str());
    }
    else {
        std::string ansiName = UTF8ToACP(name);
        std::string ansiValue = UTF8ToACP(value);
        result = RegSetValueExA(hKey, ansiName.c_str(), 0, REG_SZ, (const BYTE*)ansiValue.c_str(), (DWORD)ansiValue.size() + 1);
    }
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) return false;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    return true;
}