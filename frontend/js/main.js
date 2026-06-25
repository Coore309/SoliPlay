// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

document.getElementById('global-settings-btn').addEventListener('click', () => showView('settings'));
document.getElementById('avatar-library-btn').addEventListener('click', () => showView('avatar'));

// 启动时检查公告
window.addEventListener('DOMContentLoaded', () => {
    checkAnnouncement();
});

// 初始化：默认显示启动页
showView('start');
userInput.disabled = true;
sendBtn.disabled = true;