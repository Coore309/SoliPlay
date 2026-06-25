// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

async function loadSettings() {
    try {
        const res = await fetch('/api/config');
        const config = await res.json();
        document.getElementById('api-key-input').value = '';
        document.getElementById('debug-mode').checked = (config.debug === 1);
        document.getElementById('retry-count').value = config.retry_count || 3;
        document.getElementById('auto-save').checked = (config.auto_save !== false);
        document.getElementById('llm-log-to-story').checked = config.llm_log_to_story || false;
        document.getElementById('llm-log-story-option').style.display = (config.debug === 1) ? 'block' : 'none';
        if (config.has_api_key) {
            document.getElementById('api-key-input').placeholder = '已设置，输入新 Key 覆盖';
        } else {
            document.getElementById('api-key-input').placeholder = '输入你的 DeepSeek API Key';
        }
    } catch (e) {
        document.getElementById('api-key-input').value = localStorage.getItem('soliplay_api_key') || '';
        document.getElementById('debug-mode').checked = localStorage.getItem('soliplay_debug') === 'true';
        document.getElementById('retry-count').value = localStorage.getItem('soliplay_retries') || 3;
        document.getElementById('auto-save').checked = localStorage.getItem('soliplay_autosave') !== 'false';
        document.getElementById('llm-log-to-story').checked = localStorage.getItem('soliplay_llm_log_story') === 'true';
        document.getElementById('llm-log-story-option').style.display = document.getElementById('debug-mode').checked ? 'block' : 'none';
    }
}
loadSettings();

document.getElementById('debug-mode').addEventListener('change', (e) => {
    document.getElementById('llm-log-story-option').style.display = e.target.checked ? 'block' : 'none';
});

document.getElementById('settings-back-btn').addEventListener('click', () => showView('start'));

document.getElementById('save-settings-btn').addEventListener('click', async() => {
    const apiKey = document.getElementById('api-key-input').value.trim();
    const debugEnabled = document.getElementById('debug-mode').checked;
    const retryCount = parseInt(document.getElementById('retry-count').value) || 3;
    const autoSave = document.getElementById('auto-save').checked;
    const llmLogToStory = document.getElementById('llm-log-to-story').checked;

    localStorage.setItem('soliplay_debug', debugEnabled);
    localStorage.setItem('soliplay_retries', retryCount);
    localStorage.setItem('soliplay_autosave', autoSave);
    localStorage.setItem('soliplay_llm_log_story', llmLogToStory);

    try {
        const res = await fetch('/api/save_config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                api_key: apiKey,
                debug: debugEnabled ? 1 : 0,
                retry_count: retryCount,
                auto_save: autoSave,
                llm_log_to_story: llmLogToStory
            })
        });
        if (res.ok) {
            showToast('设置已保存', 'success');
            showView('start');
        } else {
            const err = await res.json();
            showToast('保存失败：' + (err.error || '未知错误'), 'error');
        }
    } catch (e) {
        showToast('网络错误，保存失败', 'error');
    }
});

// 绑定右下角帮助按钮
document.getElementById('help-btn').addEventListener('click', openHelp);

// 检查更新按钮
document.getElementById('check-update-btn').addEventListener('click', checkForUpdate);

// 联系作者按钮
document.getElementById('contact-author-btn').addEventListener('click', showContactModal);