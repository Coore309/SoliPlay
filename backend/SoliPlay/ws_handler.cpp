#include "ws_handler.h"
#include <iostream>

void WSHandler::handle_message(WebSocketSession& session, const std::string& raw) {
    nlohmann::json msg;
    try {
        msg = nlohmann::json::parse(raw);
    }
    catch (...) {
        session.send({ {"type", "error"}, {"message", "无效的 JSON"} });
        return;
    }

    std::string type = msg.value("type", "");

    if (type == "ping") {
        session.send({ {"type", "pong"} });
    }
    else if (type == "set_config") {
        // 这里后续会调用 Config 更新
        session.send({ {"type", "config_updated"} });
    }
    else {
        session.send({ {"type", "error"}, {"message", "未知指令: " + type} });
    }
}