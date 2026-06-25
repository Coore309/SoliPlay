// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <nlohmann/json.hpp>
#include <string>

struct CheckResult { int roll; bool success; };

CheckResult DiceCheck(const nlohmann::json& character, const std::string& skill, int difficulty);