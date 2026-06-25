// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <direct.h>
#include <windows.h>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

std::string Logger::CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::Init(const std::string& logDir) {
    m_logDir = logDir;
    _mkdir(logDir.c_str());
    std::string filename = m_logDir + "\\core_" + CurrentTimestamp() + ".log";
    // 替换文件名中的非法字符
    for (auto& c : filename) {
        if (c == ':') c = '-';
    }
    m_coreLog.open(filename, std::ios::out | std::ios::app);
    if (m_coreLog.is_open()) {
        m_coreLog << "[" << CurrentTimestamp() << "] Core log started." << std::endl;
    }
}

void Logger::Info(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_coreLog.is_open()) {
        m_coreLog << "[" << CurrentTimestamp() << "] INFO: " << msg << std::endl;
    }
}

void Logger::Error(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_coreLog.is_open()) {
        m_coreLog << "[" << CurrentTimestamp() << "] ERROR: " << msg << std::endl;
    }
}

void Logger::LLMLog(const std::string& storyId, const std::string& content) {
    if (!m_llmLogEnabled) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string storyDir = m_logDir + "\\..\\data\\stories\\" + storyId; // 相对于 logDir 定位
    _mkdir(storyDir.c_str());
    std::string filename = storyDir + "\\llm_" + CurrentTimestamp() + ".log";
    for (auto& c : filename) {
        if (c == ':') c = '-';
    }
    std::ofstream out(filename, std::ios::out | std::ios::app);
    if (out.is_open()) {
        out << "[" << CurrentTimestamp() << "]\n" << content << std::endl;
    }
}