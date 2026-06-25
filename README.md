# SoliPlay

AI 导演驱动的互动叙事引擎

支持跑团与小说模式，通过 AI 导演调度多个 AI 演员扮演角色，实现沉浸式对话。

## 快速开始

1. 从 Releases 下载绿色版压缩包，解压到任意目录。
2. 双击根目录下的 `SoliPlay` 快捷方式启动（或直接运行 `SoliPlay.exe`）。
3. 首次使用需在设置页面填入 DeepSeek API Key，前往 DeepSeek 开放平台获取。
4. 浏览器访问 `http://localhost:8080` 开始游戏。

## 主要特性

- AI 导演控制剧情走向，调度多个 AI 演员扮演角色
- 支持跑团（TRPG）和小说两种模式
- D100 判定系统，小说模式可触发“奇迹”
- 角色认知系统，称呼随主角感知变化
- 故事自动保存/加载，支持多故事管理
- 头像管理（裁剪、文件夹分类、与角色绑定）

## 项目结构

    SoliPlay/
    ├── backend/
    ├── frontend/
    ├── data/
    ├── backend/logs/
    └── README.md

## 构建

### 依赖

- C++11 编译器
- cpp-httplib（需启用 OpenSSL）
- nlohmann/json
- Windows：链接 `ws2_32.lib`、`crypt32.lib`

### 编译

使用 vcpkg 安装依赖：

    vcpkg install cpp-httplib[openssl] nlohmann-json

使用 CMake 构建：

    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release

或使用 Visual Studio，确保定义预处理器宏 `CPPHTTPLIB_OPENSSL_SUPPORT`。

## 使用说明

- 启动后点击“开始新故事”设计世界观与主角设定，即可开始冒险。
- 在聊天框输入行动或对话推进剧情，旁白和角色发言由 AI 实时生成。
- 通过侧边栏管理故事（加载、重命名、删除、抢救）。
- 头像库支持上传、裁剪、移动和删除，可绑定给角色。
- 设置中可调整重试次数、开启大模型调试日志等。

## 许可证

本项目采用 GNU General Public License v3.0 协议。

Copyright (C) 2026 Coore309

This program is free software： you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

详见 LICENSE 文件。