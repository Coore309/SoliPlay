// config.h
#pragma once
#include <string>
#include <nlohmann/json.hpp>

class Config {
public:
    std::string api_key;
    int max_conversations = 50;
    int port = 8080;

    bool load(const std::string& path = "data/config.json");
    bool save(const std::string& path = "data/config.json");
};