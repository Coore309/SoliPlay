// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// ==================== 元素引用 ====================
const avatarUploadInput = document.getElementById('avatar-upload-input');
const avatarUploadBtn = document.getElementById('avatar-upload-btn');
const cropContainer = document.getElementById('crop-container');
const cropImage = document.getElementById('crop-image');
const cropCancelBtn = document.getElementById('crop-cancel-btn');
const cropConfirmBtn = document.getElementById('crop-confirm-btn');
const avatarGrid = document.getElementById('avatar-grid');
const folderSelect = document.getElementById('folder-select');
const createFolderBtn = document.getElementById('create-folder-btn');
const deleteFolderBtn = document.getElementById('delete-folder-btn');
const avatarBackBtn = document.getElementById('avatar-back-btn');

let cropper = null;
let pendingCharacterId = null;
window._avatarSelectCallback = null;

// ==================== 上传、裁剪逻辑 ====================
avatarUploadBtn.addEventListener('click', () => avatarUploadInput.click());
avatarUploadInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (event) => {
        cropImage.src = event.target.result;
        cropContainer.style.display = 'block';
        avatarGrid.style.display = 'none';
        destroyCropper();
        cropper = new Cropper(cropImage, {
            aspectRatio: 1,
            viewMode: 1,
            autoCropArea: 1,
        });
    };
    reader.readAsDataURL(file);
});

cropCancelBtn.addEventListener('click', () => {
    destroyCropper();
    cropContainer.style.display = 'none';
    avatarGrid.style.display = 'flex';
    avatarUploadInput.value = '';
});

cropConfirmBtn.addEventListener('click', () => {
    if (!cropper) return;
    const canvas = cropper.getCroppedCanvas({ width: 256, height: 256 });
    canvas.toBlob(async(blob) => {
        const reader = new FileReader();
        reader.onloadend = async() => {
            try {
                const folder = folderSelect.value;
                const res = await fetch('/api/upload_avatar', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ data_url: reader.result, folder: folder })
                });
                const result = await res.json();
                if (result.url) {
                    showToast('头像上传成功', 'success');
                    destroyCropper();
                    cropContainer.style.display = 'none';
                    avatarGrid.style.display = 'flex';
                    avatarUploadInput.value = '';
                    loadAvatars();
                } else {
                    showToast('上传失败', 'error');
                }
            } catch (err) { showToast('网络错误', 'error'); }
        };
        reader.readAsDataURL(blob);
    }, 'image/png');
});

function destroyCropper() {
    if (cropper) { cropper.destroy();
        cropper = null; }
}

// ==================== 文件夹管理 ====================
async function loadFolders() {
    const res = await fetch('/api/avatar_folders');
    const folders = await res.json();
    folderSelect.innerHTML = '<option value="">默认文件夹</option>';
    folders.forEach(f => {
        const option = document.createElement('option');
        option.value = f;
        option.textContent = f;
        folderSelect.appendChild(option);
    });
}

createFolderBtn.addEventListener('click', () => {
    showInputModal('请输入文件夹名称：', '新建文件夹', async(name) => {
        if (name) {
            await fetch('/api/create_avatar_folder', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ folder_name: name })
            });
            loadFolders();
            showToast('文件夹已创建', 'success');
        }
    });
});

deleteFolderBtn.addEventListener('click', async() => {
    const folder = folderSelect.value;
    if (!folder) return showToast('请先选择一个文件夹', 'info');
    showConfirmModal(`确定删除文件夹「${folder}」吗？<br><small style="color:#999;">只能删除空文件夹</small>`, async(confirmed) => {
        if (confirmed) {
            const res = await fetch('/api/delete_avatar_folder', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ folder })
            });
            const data = await res.json();
            if (data.success) {
                showToast('文件夹已删除', 'success');
                folderSelect.value = '';
                loadFolders();
                loadAvatars();
            } else {
                showToast(data.error || '删除失败', 'error');
            }
        }
    });
});

folderSelect.addEventListener('change', () => loadAvatars());

// ==================== 头像列表渲染（管理视图，带右键菜单） ====================
async function loadAvatars(callback = null) {
    const folder = folderSelect.value;
    const url = folder ? `/api/avatars?folder=${encodeURIComponent(folder)}` : '/api/avatars';
    const res = await fetch(url);
    const avatars = await res.json();
    avatarGrid.innerHTML = '';
    avatars.forEach(avatarUrl => {
        const div = document.createElement('div');
        div.className = 'avatar-item';
        const img = document.createElement('img');
        img.src = avatarUrl;
        img.onclick = () => {
            if (callback) {
                if (window._pendingSelectedUrl === avatarUrl) {
                    window._pendingSelectedUrl = null;
                    div.classList.remove('selected');
                } else {
                    document.querySelectorAll('.avatar-item.selected').forEach(el => el.classList.remove('selected'));
                    window._pendingSelectedUrl = avatarUrl;
                    div.classList.add('selected');
                }
                return;
            }
        };
        div.appendChild(img);

        if (!callback) {
            const menuTrigger = document.createElement('button');
            menuTrigger.className = 'avatar-menu-trigger';
            menuTrigger.innerHTML = '⋮';
            menuTrigger.onclick = (e) => {
                e.stopPropagation();
                showAvatarContextMenu(e, avatarUrl, folder);
            };
            div.appendChild(menuTrigger);
        }

        avatarGrid.appendChild(div);
    });
}

// ==================== 上下文菜单（移动/删除） ====================
let currentContextMenu = null;

function showAvatarContextMenu(event, avatarUrl, currentFolder) {
    removeContextMenu();
    const menu = document.createElement('div');
    menu.className = 'character-menu-dropdown';
    menu.style.position = 'fixed';
    menu.style.top = event.clientY + 'px';
    menu.style.left = event.clientX + 'px';
    menu.innerHTML = `
        <button class="menu-item move-btn">📁 移动到...</button>
        <button class="menu-item delete-btn">🗑️ 删除</button>
    `;
    document.body.appendChild(menu);
    currentContextMenu = menu;

    menu.querySelector('.move-btn').addEventListener('click', (e) => {
        e.stopPropagation();
        showMoveSubmenu(avatarUrl, currentFolder, menu);
    });

    menu.querySelector('.delete-btn').addEventListener('click', async() => {
        removeContextMenu();
        showConfirmModal('确定删除该头像吗？此操作不可撤销。', async(confirmed) => {
            if (confirmed) {
                const filename = avatarUrl.split('/').pop();
                await fetch('/api/delete_avatar', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ filename, folder: currentFolder })
                });
                loadAvatars();
                showToast('头像已删除', 'success');
            }
        });
    });

    const closeHandler = (e) => {
        if (!menu.contains(e.target)) {
            removeContextMenu();
            document.removeEventListener('click', closeHandler);
        }
    };
    setTimeout(() => document.addEventListener('click', closeHandler), 0);
}

function removeContextMenu() {
    if (currentContextMenu) {
        currentContextMenu.remove();
        currentContextMenu = null;
    }
}

async function showMoveSubmenu(avatarUrl, sourceFolder, parentMenu) {
    const filename = avatarUrl.split('/').pop();
    const res = await fetch('/api/avatar_folders');
    const folders = await res.json();

    const originalHTML = parentMenu.innerHTML;

    let html = '<div style="padding:4px 8px;font-size:12px;color:#999;display:flex;justify-content:space-between;">移动到 <button class="menu-back-btn" style="border:none;background:none;cursor:pointer;font-size:12px;color:var(--blue);">← 返回</button></div>';

    if (sourceFolder !== '') {
        html += `<button class="menu-item move-target-btn" data-target="">默认文件夹</button>`;
    }
    folders.forEach(f => {
        if (f === sourceFolder) return;
        html += `<button class="menu-item move-target-btn" data-target="${f}">${f}</button>`;
    });

    parentMenu.innerHTML = html;

    parentMenu.querySelector('.menu-back-btn').addEventListener('click', (e) => {
        e.stopPropagation();
        parentMenu.innerHTML = originalHTML;
        rebindMainMenuEvents(parentMenu, avatarUrl, sourceFolder);
    });

    parentMenu.querySelectorAll('.move-target-btn').forEach(btn => {
        btn.addEventListener('click', async(e) => {
            e.stopPropagation();
            const targetFolder = btn.dataset.target;
            await moveAvatar(filename, sourceFolder, targetFolder);
            removeContextMenu();
        });
    });
}

function rebindMainMenuEvents(menu, avatarUrl, currentFolder) {
    menu.querySelector('.move-btn').addEventListener('click', (e) => {
        e.stopPropagation();
        showMoveSubmenu(avatarUrl, currentFolder, menu);
    });
    menu.querySelector('.delete-btn').addEventListener('click', async() => {
        removeContextMenu();
        showConfirmModal('确定删除该头像吗？此操作不可撤销。', async(confirmed) => {
            if (confirmed) {
                const filename = avatarUrl.split('/').pop();
                await fetch('/api/delete_avatar', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ filename, folder: currentFolder })
                });
                loadAvatars();
                showToast('头像已删除', 'success');
            }
        });
    });
}

async function moveAvatar(filename, sourceFolder, targetFolder) {
    try {
        const res = await fetch('/api/move_avatar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ filename, source_folder: sourceFolder, target_folder: targetFolder })
        });
        const data = await res.json();
        if (data.success) {
            showToast('移动成功', 'success');
            folderSelect.value = targetFolder;
            await loadAvatars();
        } else {
            showToast(data.error || '移动失败', 'error');
        }
    } catch (e) {
        showToast('网络错误', 'error');
    }
}

// ==================== 头像选择次级弹窗（使用按钮式文件夹切换） ====================
function openAvatarSelectModal(callback) {
    const overlay = document.createElement('div');
    overlay.className = 'modal active';
    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 480px;">
            <div class="modal-header">
                <h3>🖼️ 选择头像</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div class="folder-bar" style="display:flex; align-items:center; gap:8px; margin-bottom:12px;">
                <div style="position:relative; flex:1;">
                    <button id="select-folder-btn" class="btn-text" style="width:100%; display:flex; justify-content:space-between; align-items:center;">
                        <span id="current-folder-name">默认文件夹</span>
                        <span>▾</span>
                    </button>
                    <div id="select-folder-dropdown" class="menu-dropdown" style="display:none; position:absolute; top:100%; left:0; width:100%; max-height:200px; overflow-y:auto;"></div>
                </div>
            </div>
            <div id="select-avatar-grid" style="display:flex; flex-wrap:wrap; gap:10px;"></div>
            <div style="display:flex; justify-content:space-between; margin-top:12px;">
                <button id="select-cancel-btn" class="btn-text">取消</button>
                <button id="select-confirm-btn" class="btn-primary">确定选择</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);

    const folderBtn = overlay.querySelector('#select-folder-btn');
    const folderNameSpan = overlay.querySelector('#current-folder-name');
    const folderDropdown = overlay.querySelector('#select-folder-dropdown');
    const gridEl = overlay.querySelector('#select-avatar-grid');
    const confirmBtn = overlay.querySelector('#select-confirm-btn');
    const cancelBtn = overlay.querySelector('#select-cancel-btn');
    const closeBtn = overlay.querySelector('.close-modal');

    let currentSelectedUrl = null;
    let currentFolder = '';

    async function loadSelectFolders() {
        const res = await fetch('/api/avatar_folders');
        const folders = await res.json();
        folderDropdown.innerHTML = `<button class="menu-item folder-option" data-folder="">默认文件夹</button>`;
        folders.forEach(f => {
            const btn = document.createElement('button');
            btn.className = 'menu-item folder-option';
            btn.dataset.folder = f;
            btn.textContent = f;
            folderDropdown.appendChild(btn);
        });
        folderDropdown.querySelectorAll('.folder-option').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                const folder = btn.dataset.folder;
                currentFolder = folder;
                folderNameSpan.textContent = folder || '默认文件夹';
                folderDropdown.style.display = 'none';
                loadSelectAvatars();
            });
        });
    }

    async function loadSelectAvatars() {
        const url = currentFolder ? `/api/avatars?folder=${encodeURIComponent(currentFolder)}` : '/api/avatars';
        const res = await fetch(url);
        const avatars = await res.json();
        gridEl.innerHTML = '';
        avatars.forEach(avatarUrl => {
            const div = document.createElement('div');
            div.className = 'avatar-item';
            if (avatarUrl === currentSelectedUrl) div.classList.add('selected');
            const img = document.createElement('img');
            img.src = avatarUrl;
            img.draggable = false;
            div.appendChild(img);

            div.addEventListener('click', () => {
                if (currentSelectedUrl === avatarUrl) {
                    currentSelectedUrl = null;
                    div.classList.remove('selected');
                } else {
                    gridEl.querySelectorAll('.avatar-item.selected').forEach(el => el.classList.remove('selected'));
                    currentSelectedUrl = avatarUrl;
                    div.classList.add('selected');
                }
            });
            gridEl.appendChild(div);
        });
    }

    folderBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        folderDropdown.style.display = folderDropdown.style.display === 'none' ? 'block' : 'none';
    });

    document.addEventListener('click', (e) => {
        if (!folderBtn.contains(e.target) && !folderDropdown.contains(e.target)) {
            folderDropdown.style.display = 'none';
        }
    });

    const cleanup = (selectedUrl) => {
        overlay.remove();
        if (callback) callback(selectedUrl || null);
    };

    confirmBtn.addEventListener('click', () => {
        if (!currentSelectedUrl) {
            showToast('请先选择一个头像', 'info');
            return;
        }
        cleanup(currentSelectedUrl);
    });

    cancelBtn.addEventListener('click', () => cleanup(null));
    closeBtn.addEventListener('click', () => cleanup(null));
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cleanup(null);
    });

    loadSelectFolders().then(() => loadSelectAvatars());
}

// ==================== 角色头像更改弹窗 ====================
function openAvatarChangeModal(characterId, characterName) {
    const modal = document.createElement('div');
    modal.className = 'modal active';
    modal.innerHTML = `
        <div class="modal-card" style="max-width: 480px;">
            <div class="modal-header">
                <h3>🖼️ 更改「${characterName}」的头像</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div style="padding: 16px 0;">
                <label class="input-label">建议提示词（AI 生成）</label>
                <textarea id="avatar-prompt-text" style="width:100%; padding: 8px 12px; border:1px solid #ddd; border-radius:8px; min-height:80px;" placeholder="点击下方按钮生成..."></textarea>
                <button id="generate-prompt-btn" class="btn-text" style="margin-top:8px;">🎨 生成提示词</button>
                <div style="margin-top:16px; display: flex; align-items: center; gap: 8px;">
                    <button id="select-from-library-btn" class="btn-primary">🖼️ 从头像库选择</button>
                    <img id="preview-selected-avatar" src="" style="width:40px; height:40px; border-radius:50%; object-fit:cover; display:none;">
                </div>
                <div style="margin-top:12px; display:flex; justify-content:flex-end; gap:8px;">
                    <button id="remove-avatar-btn" class="btn-text" style="color:#c62828;">移除头像</button>
                    <button id="apply-avatar-btn" class="btn-primary">确定</button>
                </div>
            </div>
        </div>
    `;
    document.body.appendChild(modal);

    let selectedUrl = null;
    const previewImg = modal.querySelector('#preview-selected-avatar');
    const applyBtn = modal.querySelector('#apply-avatar-btn');
    const closeModal = () => modal.remove();

    modal.querySelector('.close-modal').addEventListener('click', closeModal);
    modal.addEventListener('click', (e) => { if (e.target === modal) closeModal(); });

    // 生成提示词
    modal.querySelector('#generate-prompt-btn').addEventListener('click', async() => {
        const promptArea = modal.querySelector('#avatar-prompt-text');
        promptArea.value = '生成中...';
        try {
            const res = await fetch('/api/generate_avatar_prompt', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ character_id: characterId })
            });
            const data = await res.json();
            promptArea.value = data.prompt || '生成失败';
        } catch (e) {
            promptArea.value = '网络错误';
        }
    });

    // 从头像库选择
    modal.querySelector('#select-from-library-btn').addEventListener('click', () => {
        openAvatarSelectModal((url) => {
            if (url) {
                selectedUrl = url;
                previewImg.src = url;
                previewImg.style.display = 'inline-block';
            }
        });
    });

    // 移除头像
    modal.querySelector('#remove-avatar-btn').addEventListener('click', async() => {
        showConfirmModal('确认移除该角色头像吗？将恢复为默认头像。', async(confirmed) => {
            if (confirmed) {
                await setCharacterAvatar(characterId, '/asset/profile/default.png');
                closeModal();
            }
        });
    });

    // 确定应用
    applyBtn.addEventListener('click', async() => {
        if (!selectedUrl) return showToast('请先选择一个头像', 'info');
        await setCharacterAvatar(characterId, selectedUrl);
        closeModal();
    });
}

// ==================== 核心：更新头像并持久化 ====================
async function setCharacterAvatar(characterId, avatarUrl) {
    // 更新聊天界面中该角色所有头像
    document.querySelectorAll(`.character-row[data-character-id="${characterId}"] .character-avatar`).forEach(img => {
        img.src = avatarUrl;
    });
    // 发送后端保存
    try {
        await fetch('/api/set_character_avatar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ character_id: characterId, avatar_url: avatarUrl })
        });
        showToast('头像已更新', 'success');
    } catch (e) {
        showToast('头像保存失败', 'error');
    }
}

// 兼容旧接口
function openAvatarLibrary(characterId) {
    const nameElement = document.querySelector(`.character-row[data-character-id="${characterId}"] .character-name`);
    const name = nameElement ? nameElement.textContent : '角色';
    openAvatarChangeModal(characterId, name);
}

// ==================== 视图初始化 ====================
function initAvatarView() {
    loadFolders();
    loadAvatars();
}

// 返回按钮
avatarBackBtn.addEventListener('click', () => {
    window._avatarSelectCallback = null;
    showView('chat');
});