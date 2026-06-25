// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "api_handler.h"
#include <nlohmann/json.hpp>
#include "sse_handler.h"
#include "debug.h"

static StoryContext* g_stream_ctx = nullptr;

static void HandleSSEConnect(const httplib::Request&, httplib::Response& res) {
    DEBUG_LOG(u8"[SSE] 新连接\n");
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    auto provider = [&](size_t offset, httplib::DataSink& sink) -> bool {
        std::lock_guard<std::mutex> lock(sseMutex);
        ssePush = [&sink](const std::string& data) {
            std::string sseData = "data: " + data + "\n\n";
            sink.write(sseData.c_str(), sseData.size());
            };
        return true;
        };
    res.set_chunked_content_provider("text/event-stream", provider,
        [](bool success) {
            std::lock_guard<std::mutex> lock(sseMutex);
            ssePush = nullptr;
            DEBUG_LOG(u8"[SSE] 连接关闭\n");
        }
    );
}

static void HandleGetStatus(const httplib::Request&, httplib::Response& res) {
    DEBUG_LOG(u8"[API] 请求主角状态\n");
    if (g_stream_ctx) {
        res.set_content(g_stream_ctx->protagonistData.dump(), "application/json");
    }
}

void RegisterStreamRoutes(httplib::Server& svr, StoryContext& ctx) {
    g_stream_ctx = &ctx;
    svr.Get("/api/stream", HandleSSEConnect);
    svr.Get("/api/status", HandleGetStatus);
}