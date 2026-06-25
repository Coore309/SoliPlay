// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "llm.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <windows.h>

// 从环境变量或配置文件获取 API Key
std::string GetApiKey() {
    const char* key = std::getenv("DEEPSEEK_API_KEY");
    if (key) return std::string(key);
    // 尝试从主程序的 config.json 读取（假设 backdoor 与 SoliPlay.exe 同目录）
    std::ifstream cfg("..\\data\\config.json");
    if (cfg) {
        // 简单解析（可不依赖 nlohmann/json，但为简单，此处手动查找）
        std::string line;
        while (std::getline(cfg, line)) {
            size_t pos = line.find("\"api_key\"");
            if (pos != std::string::npos) {
                size_t start = line.find('\"', pos + 10);
                size_t end = line.find('\"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    return line.substr(start + 1, end - start - 1);
                }
            }
        }
    }
    return "";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: backdoor.exe <角色对话ID>" << std::endl;
        return 1;
    }
    std::string hashId = argv[1];
    std::string apiKey = GetApiKey();
    if (apiKey.empty()) {
        std::cerr << "错误: 未找到 API Key。请设置环境变量 DEEPSEEK_API_KEY 或确保主程序已配置。" << std::endl;
        return 1;
    }

    // 加载角色现有对话历史（从主程序的数据目录）
    // 需要读取 ../data/stories/<当前故事ID>/character_histories.json 中的对应条目
    // 为简化，本后门假设用户知道当前故事 ID，但我们可以不加载历史，直接从头创建新会话。
    // 但为了修正认知，我们最好加载当前历史，并在 system 消息中追加修正。
    // 因为独立程序无法轻易知道当前故事 ID，我们改为让用户手动输入 system 消息，然后直接进行单轮对话。

    // 创建 LLM 实例
    llm::LLM llm(apiKey, "deepseek-v4-flash", 30);
    llm.set_utf8_output(true);

    // 提示用户输入 system 消息
    std::cout << "请输入要注入的 system 消息（修正人设的指令），按回车确认: ";
    std::string systemMsg;
    std::getline(std::cin, systemMsg);

    llm.create_conversation(hashId);
    llm.set_current_conversation(hashId);
    // 先发送 system 消息
    llm.send_message(systemMsg, "system");

    // 进入交互式对话
    std::cout << "后门已开启，现在你可以直接与角色对话。输入 /exit 退出。" << std::endl;
    while (true) {
        std::cout << "你: ";
        std::string input;
        std::getline(std::cin, input);
        if (input == "/exit") break;
        if (input.empty()) continue;

        llm.send_message(input);
        std::string reply = llm.get_reply();
        std::cout << "角色: " << reply << std::endl;
    }

    // 对话结束后，用户需要手动将修正后的对话历史存回主程序（本后门不直接写文件，但可通过后续功能实现）
    std::cout << "后门对话结束。如需持久化，请将对话历史手动合并到角色历史文件。" << std::endl;
    return 0;
}