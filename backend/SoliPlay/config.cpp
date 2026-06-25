// config.cpp
#include "config.h"
#include <fstream>
#include <iostream>

bool Config::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "配置文件不存在，将创建默认配置: " << path << std::endl;
        return false;
    }
    try {
        nlohmann::json j;
        f >> j;
        api_key = j.value("api_key", "");
        max_conversations = j.value("max_conversations", 50);
        port = j.value("port", 8080);
        return true;
    }
    catch (...) {
        std::cerr << "配置文件格式错误，使用默认值" << std::endl;
        return false;
    }
}

bool Config::save(const std::string& path) {
    nlohmann::json j;
    j["api_key"] = api_key;
    j["max_conversations"] = max_conversations;
    j["port"] = port;
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(4);
    return true;
}