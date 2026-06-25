// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// llm.h
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <random>
#include <nlohmann/json.hpp>
#include <httplib.h>

namespace llm {

    using json = nlohmann::json;

    struct Message {
        std::string role;
        std::string content;
    };

    struct Conversation {
        std::string id;
        std::string model;
        std::vector<Message> messages;
    };

    class LLM {
    public:
        LLM(const std::string& api_key,
            const std::string& model = "deepseek-v4-flash",
            int timeout = 30);

        ~LLM() = default;

        // 对话管理
        Conversation& create_conversation(const std::string& id = "");
        void set_current_conversation(const std::string& id);
        Conversation& get_current_conversation();

        // 消息收发
        void send_message(const std::string& content, const std::string& role = "user");
        std::string get_reply();

        // 流操作符
        LLM& operator<<(const std::string& msg);
        LLM& operator>>(std::string& reply);

        // 三种输出模式
        void set_json_mode(bool enable);
        void set_strict_json_schema(const std::string& function_name,
            const json& schema,
            const std::string& description = "");
        void reset_output_mode();

        // 控制输出编码：true=UTF-8，false=GBK（默认）
        void set_utf8_output(bool enable);

    private:
        std::string api_key_;
        const std::string base_url_ = "https://api.deepseek.com";
        const std::string beta_base_url_ = "https://api.deepseek.com/beta";
        std::string default_model_;
        int timeout_;

        std::unordered_map<std::string, Conversation> conversations_;
        std::string current_conv_id_;
        std::mutex mutex_;
        std::random_device rd_;
        std::mt19937 gen_;

        bool json_mode_ = false;
        bool strict_mode_ = false;
        bool use_utf8_output_ = false;   // 新增
        std::string strict_function_name_;
        json strict_schema_;
        std::string strict_description_;

        std::string generate_id();
        std::string send_request(const json& request_body, bool use_beta = false);
    };

} // namespace llm