// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <fstream>
#include <iostream>

// 简单的日志函数（写入 updater.log）
void WriteLog(const std::string& msg) {
    std::ofstream log("updater.log", std::ios::app);
    if (log.is_open()) {
        log << msg << std::endl;
    }
}

// 解压 zip 文件到当前目录（使用 PowerShell）
bool UnzipFile(const std::string& zipPath, const std::string& destDir) {
    std::string cmd = "powershell -Command \"Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + destDir + "' -Force\"";
    return system(cmd.c_str()) == 0;
}

// 启动主程序
void LaunchMainApp() {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA("SoliPlay.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WriteLog("Updater started.");

    // 解析命令行参数：updater.exe <zipFilePath>
    std::string zipFile;
    if (__argc > 1) {
        zipFile = __argv[1];
    }
    else {
        WriteLog("Error: No zip file provided.");
        return 1;
    }

    // 等待主程序完全退出（简单等待 2 秒，或通过进程检测）
    Sleep(2000);

    // 解压到当前目录
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    WriteLog("Unzipping " + zipFile + " to " + currentDir);
    if (!UnzipFile(zipFile, currentDir)) {
        WriteLog("Unzip failed.");
        // 即使失败也尝试启动，避免程序无法使用
    }

    // 删除下载的 zip 文件
    DeleteFileA(zipFile.c_str());

    // 重新启动主程序
    LaunchMainApp();

    return 0;
}