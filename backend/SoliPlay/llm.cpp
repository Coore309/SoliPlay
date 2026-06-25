// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// llm.cpp
#define _CRT_SECURE_NO_WARNINGS

#include "llm.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <windows.h>

namespace llm {

    // ====== 编码转换（GBK ↔ UTF-8） ======
    static std::string gbk_to_utf8(const std::string& gbk) {
        if (gbk.empty()) return "";
        int len = MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), -1, NULL, 0);
        std::wstring wstr(len, L'\0');
        MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), -1, &wstr[0], len);
        len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string utf8(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], len, NULL, NULL);
        utf8.pop_back();
        return utf8;
    }

    static std::string utf8_to_gbk(const std::string& utf8) {
        if (utf8.empty()) return "";
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
        std::wstring wstr(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);
        len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string gbk(len, '\0');
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &gbk[0], len, NULL, NULL);
        gbk.pop_back();
        return gbk;
    }

    static std::string current_time_str() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d%H%M%S");
        return ss.str();
    }

    // ---------- 构造函数 ----------
    LLM::LLM(const std::string& api_key,
        const std::string& model,
        int timeout)
        : api_key_(api_key), default_model_(model), timeout_(timeout), gen_(rd_()) {
        if (api_key_.empty()) {
            throw std::runtime_error("LLM: API Key cannot be empty");
        }
    }

    // ---------- 对话管理 ----------
    Conversation& LLM::create_conversation(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string conv_id = id.empty() ? generate_id() : id;
        if (conversations_.find(conv_id) != conversations_.end()) {
            throw std::runtime_error("LLM: Conversation ID already exists: " + conv_id);
        }
        Conversation conv;
        conv.id = conv_id;
        conv.model = default_model_;
        auto result = conversations_.emplace(conv_id, std::move(conv));
        current_conv_id_ = conv_id;
        return result.first->second;
    }

    void LLM::set_current_conversation(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (conversations_.find(id) == conversations_.end()) {
            throw std::runtime_error("LLM: Conversation not found: " + id);
        }
        current_conv_id_ = id;
    }

    Conversation& LLM::get_current_conversation() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_conv_id_.empty()) {
            throw std::runtime_error("LLM: No current conversation, create one first.");
        }
        auto it = conversations_.find(current_conv_id_);
        if (it == conversations_.end()) {
            throw std::runtime_error("LLM: Current conversation invalid.");
        }
        return it->second;
    }

    // ---------- 消息收发 ----------
    void LLM::send_message(const std::string& content, const std::string& role) {
        auto& conv = get_current_conversation();
        conv.messages.push_back({ role, gbk_to_utf8(content) });
    }

    std::string LLM::get_reply() {
        auto& conv = get_current_conversation();
        if (conv.messages.empty()) {
            throw std::runtime_error("LLM: No messages to reply to.");
        }

        json request_body;
        request_body["model"] = conv.model;
        request_body["messages"] = json::array();
        for (const auto& msg : conv.messages) {
            request_body["messages"].push_back({
                {"role", msg.role},
                {"content", msg.content}
                });
        }
        request_body["stream"] = false;

        bool use_beta = false;

        // 模式选择
        if (strict_mode_) {
            // 严格模式：需关闭思考
            request_body["thinking"] = { {"type", "disabled"} };

            json tool = {
                {"type", "function"},
                {"function", {
                    {"name", strict_function_name_},
                    {"description", strict_description_},
                    {"parameters", strict_schema_},
                    {"strict", true}
                }}
            };
            request_body["tools"] = json::array({ tool });
            request_body["tool_choice"] = {
                {"type", "function"},
                {"function", {{"name", strict_function_name_}}}
            };
            use_beta = true;
        }
        else if (json_mode_) {
            request_body["response_format"] = { {"type", "json_object"} };
        }

        std::string response_str = send_request(request_body, use_beta);
        json response_json = json::parse(response_str);

        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            throw std::runtime_error("LLM: API response missing 'choices'.");
        }

        auto choice = response_json["choices"][0];
        std::string reply_utf8;

        if (strict_mode_) {
            if (!choice.contains("message") || !choice["message"].contains("tool_calls")) {
                throw std::runtime_error("LLM: Strict mode expected tool_calls but not found.");
            }
            auto tool_calls = choice["message"]["tool_calls"];
            if (tool_calls.empty()) {
                throw std::runtime_error("LLM: No tool_calls in response.");
            }
            auto args = tool_calls[0]["function"]["arguments"];
            if (!args.is_string()) {
                throw std::runtime_error("LLM: tool_call arguments not a string.");
            }
            reply_utf8 = args.get<std::string>();
        }
        else {
            if (!choice.contains("message") || !choice["message"].contains("content")) {
                throw std::runtime_error("LLM: API response missing message content.");
            }
            reply_utf8 = choice["message"]["content"].get<std::string>();
        }

        conv.messages.push_back({ "assistant", reply_utf8 });

        // 根据输出编码设置返回
        if (use_utf8_output_) {
            return reply_utf8;      // 直接返回 UTF-8
        }
        else {
            return utf8_to_gbk(reply_utf8); // 转为 GBK
        }
    }

    LLM& LLM::operator<<(const std::string& msg) {
        send_message(msg);
        return *this;
    }

    LLM& LLM::operator>>(std::string& reply) {
        reply = get_reply();
        return *this;
    }

    // ===== 三种模式接口 =====
    void LLM::set_json_mode(bool enable) {
        json_mode_ = enable;
        if (enable) strict_mode_ = false;
    }

    void LLM::set_strict_json_schema(const std::string& function_name,
        const json& schema,
        const std::string& description) {
        strict_mode_ = true;
        strict_function_name_ = function_name;
        strict_schema_ = schema;
        strict_description_ = description;
        json_mode_ = false;
    }

    void LLM::reset_output_mode() {
        json_mode_ = false;
        strict_mode_ = false;
    }

    // ===== 新增：设置输出编码 =====
    void LLM::set_utf8_output(bool enable) {
        use_utf8_output_ = enable;
    }

    // ---------- 内部辅助函数 ----------
    std::string LLM::generate_id() {
        // 注意：调用者已持有锁，此处不加锁
        std::string base = "conv_" + current_time_str() + "_";
        std::uniform_int_distribution<> dist(1000, 9999);
        return base + std::to_string(dist(gen_));
    }

    std::string LLM::send_request(const json& request_body, bool use_beta) {
        const std::string& url = use_beta ? beta_base_url_ : base_url_;
        httplib::Client client(url);
        client.set_connection_timeout(timeout_);
        client.set_read_timeout(timeout_);
        client.set_decompress(true);

        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + api_key_}
        };

        std::string body = request_body.dump();
        auto res = client.Post("/v1/chat/completions", headers, body, "application/json");

        if (!res) {
            throw std::runtime_error("LLM: HTTP request failed: " + httplib::to_string(res.error()));
        }
        if (res->status != 200) {
            throw std::runtime_error("LLM: HTTP error " + std::to_string(res->status) + ": " + res->body);
        }

        return res->body;
    }

} // namespace llm