// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

const sidebar = document.getElementById('sidebar');
document.getElementById('sidebar-toggle-btn').addEventListener('click', () => {
    sidebar.classList.add('open');
    fetchStoryList();
});
document.getElementById('sidebar-close').addEventListener('click', () => sidebar.classList.remove('open'));

// ========== 故事列表 ==========
async function fetchStoryList() {
    try {
        const res = await fetch('/api/stories');
        const stories = await res.json();
        let pinned = JSON.parse(localStorage.getItem('pinnedStories') || '[]');
        stories.sort((a, b) => {
            const aPin = pinned.includes(a.id) ? 1 : 0;
            const bPin = pinned.includes(b.id) ? 1 : 0;
            if (aPin !== bPin) return bPin - aPin;
            return (b.last_played || 0) - (a.last_played || 0);
        });

        const listDiv = document.getElementById('story-list');
        listDiv.innerHTML = '';
        if (stories.length === 0) {
            listDiv.innerHTML = '<div style="padding:16px;color:#999;">暂无故事</div>';
            return;
        }
        stories.forEach(story => {
            const item = document.createElement('div');
            item.className = 'story-item';
            const lastPlayed = new Date(story.last_played * 1000).toLocaleString();
            item.innerHTML = `
                <div class="story-item-content" data-id="${story.id}">
                    <b>${story.title || '未命名'}</b><br><small>${lastPlayed}</small>
                </div>
                <div class="story-item-menu">
                    <button class="menu-trigger" data-id="${story.id}">⋮</button>
                    <div class="menu-dropdown" style="display:none;">
                        <button class="menu-item pin-btn" data-id="${story.id}">${pinned.includes(story.id) ? '取消置顶' : '置顶'}</button>
                        <button class="menu-item rename-btn" data-id="${story.id}">重命名</button>
                        <button class="menu-item delete-btn" data-id="${story.id}">删除</button>
                        <button class="menu-item open-folder-btn" data-id="${story.id}">📁 打开文件夹</button>
                        <button class="menu-item rescue-btn" data-id="${story.id}" style="color:#e67e22;">🚑 抢救故事</button>
                        <button class="menu-item export-btn" data-id="${story.id}">📤 导出故事</button>
                    </div>
                </div>
            `;
            item.querySelector('.story-item-content').addEventListener('click', () => loadStory(story.id));
            const trigger = item.querySelector('.menu-trigger');
            const dropdown = item.querySelector('.menu-dropdown');
            trigger.addEventListener('click', (e) => {
                e.stopPropagation();
                document.querySelectorAll('.menu-dropdown').forEach(d => { if (d !== dropdown) d.style.display = 'none'; });
                const rect = trigger.getBoundingClientRect();
                const dropdownWidth = 120;
                const viewportWidth = window.innerWidth;
                let top = rect.bottom + 4;
                let left = rect.left;
                if (left + dropdownWidth > viewportWidth - 8) left = viewportWidth - dropdownWidth - 8;
                if (top + 100 > window.innerHeight) top = rect.top - 104;
                dropdown.style.position = 'fixed';
                dropdown.style.top = top + 'px';
                dropdown.style.left = left + 'px';
                dropdown.style.display = dropdown.style.display === 'none' ? 'block' : 'none';
            });
            // 置顶
            item.querySelector('.pin-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                togglePin(story.id);
                dropdown.style.display = 'none';
            });
            // 重命名
            item.querySelector('.rename-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                dropdown.style.display = 'none';
                showInputModal('请输入新标题', '重命名故事', (newTitle) => {
                    if (newTitle) renameStory(story.id, newTitle);
                });
            });
            // 删除
            item.querySelector('.delete-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                dropdown.style.display = 'none';
                showConfirmModal('确定删除这个故事吗？此操作不可撤销。', (confirmed) => {
                    if (confirmed) deleteStory(story.id);
                });
            });
            // 打开文件夹
            item.querySelector('.open-folder-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                openStoryFolder(story.id);
                dropdown.style.display = 'none';
            });
            // 抢救故事
            item.querySelector('.rescue-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                dropdown.style.display = 'none';
                showConfirmModal('确定要抢救这个故事吗？<br><small style="color:#999;">将清空导演和角色对话记忆，但保留角色档案、主角状态和世界观，并根据最近的聊天记录重新生成衔接剧情。</small>', (confirmed) => {
                    if (confirmed) rescueStory(story.id);
                });
            });
            // 导出故事
            item.querySelector('.export-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                dropdown.style.display = 'none';
                exportStory(story.id);
            });
            listDiv.appendChild(item);
        });
    } catch (err) {
        console.error('获取故事列表失败', err);
        showToast('无法获取故事列表', 'error');
    }
}

document.addEventListener('click', () => {
    document.querySelectorAll('.menu-dropdown').forEach(d => d.style.display = 'none');
});

function togglePin(storyId) {
    let pinned = JSON.parse(localStorage.getItem('pinnedStories') || '[]');
    const index = pinned.indexOf(storyId);
    if (index > -1) pinned.splice(index, 1);
    else pinned.push(storyId);
    localStorage.setItem('pinnedStories', JSON.stringify(pinned));
    fetchStoryList();
}

async function deleteStory(id) {
    await fetch('/api/delete_story', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ story_id: id }) });
    showToast('故事已删除', 'success');
    fetchStoryList();
}

async function renameStory(id, title) {
    await fetch('/api/rename_story', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ story_id: id, title: title }) });
    showToast('重命名成功', 'success');
    fetchStoryList();
}

async function openStoryFolder(storyId) {
    try {
        const res = await fetch('/api/open_story_folder', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ story_id: storyId })
        });
        if (res.ok) {
            showToast('文件夹已打开', 'success');
        } else {
            const err = await res.json();
            showToast('打开失败：' + (err.error || '未知错误'), 'error');
        }
    } catch (e) {
        showToast('网络错误', 'error');
    }
}

async function rescueStory(storyId) {
    try {
        showLoading();
        const res = await fetch('/api/rescue_story', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ story_id: storyId })
        });
        const data = await res.json();
        if (data.success) {
            hideLoading();
            showToast('故事已抢救，正在重新加载...', 'success');
            loadStory(storyId);
        } else {
            hideLoading();
            showToast('抢救失败：' + (data.error || '未知错误'), 'error');
        }
    } catch (e) {
        hideLoading();
        showToast('网络错误', 'error');
    }
}

// 导出故事函数
async function exportStory(storyId) {
    try {
        showToast('正在导出故事...', 'info');
        const res = await fetch('/api/export_story', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ story_id: storyId })
        });
        if (!res.ok) {
            const err = await res.json();
            showToast('导出失败：' + (err.error || '未知错误'), 'error');
            return;
        }
        const blob = await res.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        const disposition = res.headers.get('Content-Disposition');
        let filename = 'story_export.txt';
        if (disposition && disposition.indexOf('filename=') !== -1) {
            filename = disposition.split('filename=')[1].replace(/['"]/g, '');
        }
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);
        showToast('故事导出成功', 'success');
    } catch (e) {
        showToast('网络错误，导出失败', 'error');
    }
}

async function loadStory(storyId) {
    try {
        const res = await fetch('/api/load_story', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ story_id: storyId })
        });
        const data = await res.json();
        if (data.success) {
            window.currentStoryId = storyId;
            document.getElementById('chat-messages').innerHTML = '';
            data.chat_messages.forEach(msg => addMessage(msg));
            chatMessages.scrollTop = chatMessages.scrollHeight;
            const lastMsg = data.chat_messages[data.chat_messages.length - 1];
            document.getElementById('input-prompt').textContent = (lastMsg && lastMsg.type === 'input_required') ? lastMsg.prompt : '输入行动或对话...';
            userInput.disabled = false;
            sendBtn.disabled = false;
            userInput.focus();
            sidebar.classList.remove('open');
            showView('chat');
        } else {
            showToast('加载故事失败', 'error');
        }
    } catch (err) {
        console.error('加载故事失败', err);
        showToast('加载故事失败，请检查网络', 'error');
    }
}