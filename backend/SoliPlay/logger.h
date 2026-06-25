// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    static Logger& Instance();
    void Init(const std::string& logDir);
    void Info(const std::string& msg);
    void Error(const std::string& msg);
    // 大模型调试日志（故事相关）
    void LLMLog(const std::string& storyId, const std::string& content);
    void SetLLMLogEnabled(bool enabled) { m_llmLogEnabled = enabled; }
private:
    Logger() = default;
    std::mutex m_mutex;
    std::string m_logDir;
    std::ofstream m_coreLog;
    bool m_llmLogEnabled = false;
    std::string CurrentTimestamp();
};