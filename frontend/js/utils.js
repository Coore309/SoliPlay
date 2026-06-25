// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// ========== 全局工具函数 ==========

let currentLoading = null;

// ========== Toast 通知 ==========
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    let icon = '';
    if (type === 'success') icon = '✅';
    else if (type === 'error') icon = '❌';
    else if (type === 'info') icon = 'ℹ️';
    toast.innerHTML = `<span class="toast-icon">${icon}</span><span>${message}</span>`;
    container.appendChild(toast);
    setTimeout(() => {
        toast.style.animation = 'fadeOut 0.3s ease-out forwards';
        toast.addEventListener('animationend', () => {
            if (toast.parentNode) toast.parentNode.removeChild(toast);
        });
    }, 3000);
}

// ========== 加载动画 ==========
function createLoadingIndicator() {
    const div = document.createElement('div');
    div.className = 'loading-indicator';
    div.innerHTML = '<div class="spinner"></div><span>正在构建世界，请稍候...</span>';
    return div;
}

function showLoading() {
    hideLoading();
    const m = document.getElementById('chat-messages');
    while (m.firstChild) m.removeChild(m.firstChild);
    currentLoading = createLoadingIndicator();
    m.appendChild(currentLoading);
}

function hideLoading() {
    if (currentLoading && currentLoading.parentNode) {
        currentLoading.parentNode.removeChild(currentLoading);
        currentLoading = null;
    }
}

// ========== 自定义输入弹窗 ==========
function showInputModal(promptText, title, callback) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="width: 320px;">
            <div class="modal-header">
                <h3>${title}</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div>
                <label class="input-label">${promptText}</label>
                <input type="text" id="modal-input" style="width:100%; padding: 8px 12px; border:1px solid #ddd; border-radius:8px;">
            </div>
            <div class="create-actions" style="margin-top:16px;">
                <button class="btn-text cancel-modal">取消</button>
                <button class="btn-primary confirm-modal">确定</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);

    const input = overlay.querySelector('#modal-input');
    input.focus();

    const cleanup = (value) => {
        overlay.remove();
        if (callback) callback(value);
    };

    overlay.querySelector('.close-modal').addEventListener('click', () => cleanup(null));
    overlay.querySelector('.cancel-modal').addEventListener('click', () => cleanup(null));
    overlay.querySelector('.confirm-modal').addEventListener('click', () => {
        cleanup(input.value.trim());
    });
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup(null);
    });
}

// ========== 确认弹窗 ==========
function showConfirmModal(message, callback) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 320px;">
            <div class="modal-header">
                <h3>确认操作</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div style="padding: 16px 0; color: var(--text); font-size: 14px; line-height: 1.5;">
                ${message}
            </div>
            <div class="create-actions" style="margin-top: 16px;">
                <button class="btn-text cancel-modal">取消</button>
                <button class="btn-primary confirm-modal">确定</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);

    const cleanup = (result) => {
        overlay.remove();
        if (callback) callback(result);
    };

    overlay.querySelector('.close-modal').addEventListener('click', () => cleanup(false));
    overlay.querySelector('.cancel-modal').addEventListener('click', () => cleanup(false));
    overlay.querySelector('.confirm-modal').addEventListener('click', () => cleanup(true));
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup(false);
    });
}

// ========== 错误恢复弹窗 ==========
function showErrorModal(message, onReload) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 360px;">
            <div class="modal-header">
                <h3>⚠️ 发生错误</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div style="padding: 16px 0; color: var(--text); font-size: 14px; line-height: 1.5;">
                ${message}
            </div>
            <div class="create-actions" style="margin-top: 16px; display: flex; justify-content: flex-end; gap: 8px;">
                <button class="btn-text cancel-modal">取消</button>
                <button class="btn-primary reload-btn">重新加载故事</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);

    const cleanup = () => overlay.remove();
    overlay.querySelector('.close-modal').addEventListener('click', cleanup);
    overlay.querySelector('.cancel-modal').addEventListener('click', cleanup);
    overlay.querySelector('.reload-btn').addEventListener('click', () => {
        cleanup();
        if (onReload) onReload();
    });
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup();
    });
}

// ========== 公告弹窗 ==========
function showAnnouncementModal(title, content, onClose) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 400px;">
            <div class="modal-header">
                <h3>📢 ${title}</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div style="padding: 16px 0; color: var(--text); font-size: 14px; line-height: 1.8;">
                ${content.replace(/\n/g, '<br>')}
            </div>
            <div class="create-actions" style="margin-top: 16px; justify-content: flex-end;">
                <button class="btn-primary close-modal">我知道了</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);

    const cleanup = () => {
        overlay.remove();
        if (onClose) onClose();
    };
    overlay.querySelectorAll('.close-modal').forEach(btn => {
        btn.addEventListener('click', cleanup);
    });
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup();
    });
}

// ========== 帮助文档 ==========
function openHelp() {
    window.open('https://docs.qq.com/s/HMUo3d5agE0auqySBOWj3q', '_blank');
}

// ========== 检查更新 ==========
async function checkForUpdate() {
    try {
        const configRes = await fetch('/api/config');
        const config = await configRes.json();
        const currentVersion = config.version;
        const gitRes = await fetch('https://api.github.com/repos/Coore309/SoliPlay/releases/latest');
        const release = await gitRes.json();
        if (release.tag_name && release.tag_name !== currentVersion) {
            let directUrl = null;
            if (release.assets && release.assets.length > 0) {
                for (const asset of release.assets) {
                    if (asset.name === 'SoliPlay.zip') {
                        directUrl = asset.browser_download_url;
                        break;
                    }
                }
            }
            const fallbackUrl = release.html_url;
            showUpdateModal(release.name, directUrl, fallbackUrl);
        } else {
            showToast('已是最新版本', 'success');
        }
    } catch (e) {
        showToast('检查更新失败，请检查网络连接', 'error');
    }
}

function showUpdateModal(versionName, directDownloadUrl, fallbackUrl) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 360px;">
            <div class="modal-header">
                <h3>🎉 发现新版本</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div style="padding: 16px 0; color: var(--text); font-size: 14px; line-height: 1.8;">
                新版本 <b>${versionName}</b> 可用。<br>
                您可以选择自动更新或手动下载 <b>SoliPlay.zip</b> 覆盖。
            </div>
            <div class="create-actions" style="margin-top: 16px; display: flex; gap: 8px; justify-content: flex-end;">
                <button class="btn-text later-btn">以后再说</button>
                <button class="btn-primary auto-update-btn">自动更新</button>
                <button class="btn-text manual-btn">手动下载</button>
            </div>
            <div id="manual-download-options" style="display:none; margin-top:12px; display:flex; gap:8px; justify-content:flex-end;">
                <button class="btn-primary github-btn">GitHub</button>
                <button class="btn-primary gitee-btn">Gitee</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);

    const cleanup = () => overlay.remove();
    overlay.querySelector('.close-modal').addEventListener('click', cleanup);
    overlay.querySelector('.later-btn').addEventListener('click', cleanup);
    overlay.querySelector('.auto-update-btn').addEventListener('click', async() => {
        cleanup();
        await performAutoUpdate();
    });
    overlay.querySelector('.manual-btn').addEventListener('click', () => {
        const optionsDiv = overlay.querySelector('#manual-download-options');
        optionsDiv.style.display = optionsDiv.style.display === 'none' ? 'flex' : 'none';
    });
    overlay.querySelector('.github-btn').addEventListener('click', () => {
        if (directDownloadUrl) {
            window.open(directDownloadUrl, '_blank');
        } else {
            window.open(fallbackUrl, '_blank');
        }
        cleanup();
    });
    overlay.querySelector('.gitee-btn').addEventListener('click', () => {
        window.open('https://gitee.com/Coore309/SoliPlay/releases', '_blank');
        cleanup();
    });
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup();
    });
}

async function performAutoUpdate() {
    showLoading();
    try {
        const res = await fetch('/api/update', { method: 'POST' });
        const data = await res.json();
        hideLoading();
        if (data.success) {
            showToast('更新已开始，程序即将重启', 'success');
            userInput.disabled = true;
            sendBtn.disabled = true;
            setTimeout(() => {
                location.reload();
            }, 3000);
        } else {
            showToast('自动更新失败：' + (data.error || '未知错误'), 'error');
        }
    } catch (e) {
        hideLoading();
        showToast('网络错误，自动更新失败', 'error');
    }
}

// ========== 检查公告 ==========
async function checkAnnouncement() {
    try {
        const res = await fetch('https://raw.githubusercontent.com/Coore309/SoliPlay/main/announcement.json');
        if (!res.ok) return;
        const data = await res.json();
        const lastId = localStorage.getItem('soliplay_last_announcement');
        if (data.id && data.id !== lastId) {
            showAnnouncementModal(data.title, data.content, () => {
                localStorage.setItem('soliplay_last_announcement', data.id);
            });
        }
    } catch (e) { /* 网络错误静默处理 */ }
}

function showCharacterIdModal(hashId, displayName) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 360px;">
            <div class="modal-header">
                <h3>🔑 角色对话 ID</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div style="padding: 16px 0; color: var(--text); font-size: 14px; line-height: 1.8;">
                <p><b>角色：</b>${displayName}</p>
                <p><b>对话 ID：</b><code>${hashId}</code></p>
                <hr style="border: 0.5px solid var(--border); margin: 12px 0;">
                <p style="color: var(--text-secondary); font-size: 12px;">
                    如需修正此角色的认知，请使用后门程序：<br>
                    <code>backdoor.exe ${hashId}</code>
                </p>
            </div>
            <div class="create-actions" style="margin-top: 16px; justify-content: flex-end;">
                <button class="btn-primary close-modal">关闭</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
    const cleanup = () => overlay.remove();
    overlay.querySelectorAll('.close-modal').forEach(btn => btn.addEventListener('click', cleanup));
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup();
    });
}