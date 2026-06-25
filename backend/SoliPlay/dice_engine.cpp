// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "dice_engine.h"
#include <random>
#include <chrono>
#include <string>

CheckResult DiceCheck(const nlohmann::json& character, const std::string& skill, int difficulty) {
    int baseAttr = 10;

    // 战斗相关
    if (skill.find(u8"剑") != std::string::npos ||
        skill.find(u8"斧") != std::string::npos ||
        skill.find(u8"锤") != std::string::npos ||
        skill.find(u8"棍") != std::string::npos ||
        skill.find(u8"近战") != std::string::npos ||
        skill.find(u8"格斗") != std::string::npos ||
        skill.find(u8"冲撞") != std::string::npos ||
        skill.find(u8"擒抱") != std::string::npos) {
        baseAttr = character["attributes"].value("str", 10);
    }
    else if (skill.find(u8"弓") != std::string::npos ||
        skill.find(u8"弩") != std::string::npos ||
        skill.find(u8"枪") != std::string::npos ||
        skill.find(u8"远程") != std::string::npos ||
        skill.find(u8"投掷") != std::string::npos) {
        baseAttr = character["attributes"].value("dex", 10);
    }
    else if (skill.find(u8"闪避") != std::string::npos ||
        skill.find(u8"防御") != std::string::npos ||
        skill.find(u8"格挡") != std::string::npos) {
        baseAttr = character["attributes"].value("dex", 10);
    }
    // 潜行与灵巧
    else if (skill.find(u8"潜行") != std::string::npos ||
        skill.find(u8"偷窃") != std::string::npos ||
        skill.find(u8"开锁") != std::string::npos ||
        skill.find(u8"解除陷阱") != std::string::npos ||
        skill.find(u8"扒窃") != std::string::npos) {
        baseAttr = character["attributes"].value("dex", 10);
    }
    // 社交
    else if (skill.find(u8"说服") != std::string::npos ||
        skill.find(u8"魅惑") != std::string::npos ||
        skill.find(u8"威胁") != std::string::npos ||
        skill.find(u8"欺骗") != std::string::npos ||
        skill.find(u8"表演") != std::string::npos ||
        skill.find(u8"交涉") != std::string::npos ||
        skill.find(u8"收集信息") != std::string::npos) {
        baseAttr = character["attributes"].value("cha", 10);
    }
    // 感知与知识
    else if (skill.find(u8"察觉") != std::string::npos ||
        skill.find(u8"感知") != std::string::npos ||
        skill.find(u8"聆听") != std::string::npos ||
        skill.find(u8"侦查") != std::string::npos ||
        skill.find(u8"搜索") != std::string::npos ||
        skill.find(u8"调查") != std::string::npos ||
        skill.find(u8"鉴定") != std::string::npos ||
        skill.find(u8"察言观色") != std::string::npos ||
        skill.find(u8"洞察") != std::string::npos) {
        baseAttr = character["attributes"].value("int", 10);
    }
    else if (skill.find(u8"知识") != std::string::npos ||
        skill.find(u8"回忆") != std::string::npos ||
        skill.find(u8"神秘学") != std::string::npos ||
        skill.find(u8"宗教") != std::string::npos ||
        skill.find(u8"历史") != std::string::npos ||
        skill.find(u8"自然") != std::string::npos ||
        skill.find(u8"魔法") != std::string::npos ||
        skill.find(u8"鉴定") != std::string::npos) {
        baseAttr = character["attributes"].value("int", 10);
    }
    // 运动
    else if (skill.find(u8"攀爬") != std::string::npos ||
        skill.find(u8"游泳") != std::string::npos ||
        skill.find(u8"跳跃") != std::string::npos ||
        skill.find(u8"奔跑") != std::string::npos ||
        skill.find(u8"举重") != std::string::npos) {
        baseAttr = character["attributes"].value("str", 10);
    }
    // 生存与急救
    else if (skill.find(u8"生存") != std::string::npos ||
        skill.find(u8"追踪") != std::string::npos ||
        skill.find(u8"急救") != std::string::npos ||
        skill.find(u8"医疗") != std::string::npos) {
        baseAttr = character["attributes"].value("int", 10);
    }
    // 其他技术
    else if (skill.find(u8"魔法装置") != std::string::npos ||
        skill.find(u8"使用装置") != std::string::npos) {
        baseAttr = character["attributes"].value("int", 10);
    }

    int skillValue = character["skills"].value(skill, baseAttr * 2);
    int successRate = skillValue - difficulty;
    if (successRate < 5) successRate = 5;
    if (successRate > 95) successRate = 95;

    static std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(1, 100);
    int roll = dist(gen);
    return { roll, roll <= successRate };
}