// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <vector>

std::string UTF8ToACP(const std::string& utf8);
std::string ACPToUTF8(const std::string& acp);
std::string GetExeDir();
std::string GenerateHash();
std::vector<std::string> ListSubdirectories(const std::string& path);
std::string CleanUTF8(const std::string& input);
bool SetPermanentEnvVar(const std::string& name, const std::string& value);