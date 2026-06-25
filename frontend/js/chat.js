const chatMessages = document.getElementById('chat-messages');
const userInput = document.getElementById('user-input');
const sendBtn = document.getElementById('send-btn');
const evtSource = new EventSource('/api/stream');
evtSource.onmessage = e => {
    const msg = JSON.parse(e.data);
    if (msg.type === 'done') { hideLoading(); return; }
    hideLoading();
    if (msg.type === 'error') {
        const errorMsg = msg.message || '发生未知错误';
        showErrorModal(errorMsg, () => {
            if (window.currentStoryId) {
                loadStory(window.currentStoryId);
            } else {
                location.reload();
            }
        });
        return;
    }
    addMessage(msg);
};

function addMessage(msg) {
    const div = document.createElement('div');
    div.className = 'bubble';
    if (msg.type === 'narrator') {
        div.classList.add('narrator');
        div.textContent = msg.text;
    } else if (msg.type === 'character_dialogue') {
        const isProtagonist = msg.character_id === 'protagonist';
        div.classList.add('character-row');
        if (isProtagonist) div.classList.add('protagonist');
        div.dataset.characterId = msg.character_id;
        const avatar = msg.avatar_url || '/asset/profile/default.png';
        div.innerHTML = `
            <img class="character-avatar clickable" src="${avatar}" alt="${msg.name}" onerror="this.src='/asset/profile/default.png'"
                 data-character-id="${msg.character_id}">
            <div class="character-bubble-wrapper">
                <div class="character-name">${msg.name}</div>
                <div class="character-bubble">${msg.text}</div>
            </div>`;
        const avatarImg = div.querySelector('.character-avatar');
        avatarImg.addEventListener('click', (e) => {
            e.stopPropagation();
            showCharacterMenu(msg.character_id, msg.name, avatarImg);
        });
    } else if (msg.type === 'action_result') {
        const narrDiv = document.createElement('div');
        narrDiv.className = 'bubble narrator';
        let text = msg.description + ' (判定: ' + msg.roll + ', ' + (msg.success ? '成功' : '失败') + ')';
        if (msg.miracle && !msg.success) {
            text = '✨ 奇迹发生了！' + text;
        }
        narrDiv.textContent = text;
        chatMessages.appendChild(narrDiv);
        chatMessages.scrollTop = chatMessages.scrollHeight;
        return;
    } else if (msg.type === 'input_required') {
        document.getElementById('input-prompt').textContent = msg.prompt;
        userInput.disabled = false;
        sendBtn.disabled = false;
        userInput.focus();
        return;
    }
    chatMessages.appendChild(div);
    chatMessages.scrollTop = chatMessages.scrollHeight;
}

async function sendTurn(type, content, extra = {}) {
    userInput.disabled = true;
    sendBtn.disabled = true;
    try {
        const res = await fetch('/api/turn', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ type, content, ...extra }) });
        if (!res.ok) {
            const errData = await res.json();
            showToast(errData.error || '请求失败', 'error');
        }
    } catch (err) {
        console.error(err);
        showToast('网络错误，请检查连接', 'error');
        hideLoading();
        userInput.disabled = false;
        sendBtn.disabled = false;
    }
}

function sendUserInput() {
    const content = userInput.value.trim();
    if (!content) return;
    sendTurn('user_input', content);
    userInput.value = '';
}
sendBtn.addEventListener('click', sendUserInput);
userInput.addEventListener('keydown', e => {
    if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        sendUserInput();
    }
});

function showCharacterMenu(characterId, characterName, avatarElement) {
    const existing = document.querySelector('.character-menu-dropdown');
    if (existing && existing.parentNode) {
        existing.parentNode.removeChild(existing);
    }

    const menu = document.createElement('div');
    menu.className = 'character-menu-dropdown';
    menu.innerHTML = `
        <button class="menu-item view-panel-btn">📋 查看角色面板</button>
        <button class="menu-item change-avatar-btn">🖼️ 更改头像</button>
    `;
    const rect = avatarElement.getBoundingClientRect();
    menu.style.position = 'fixed';
    menu.style.top = rect.bottom + 'px';
    menu.style.left = rect.left + 'px';
    menu.style.zIndex = '2000';
    document.body.appendChild(menu);

    menu.querySelector('.view-panel-btn').addEventListener('click', () => {
        if (menu && menu.parentNode) {
            menu.parentNode.removeChild(menu);
        }
        // 获取角色对话 ID
        fetch(`/api/character_info?character_id=${characterId}`)
            .then(res => res.json())
            .then(data => {
                if (data.hash_id) {
                    showCharacterIdModal(data.hash_id, data.display_name || characterName);
                } else {
                    showToast('无法获取角色对话 ID', 'error');
                }
            })
            .catch(() => showToast('网络错误', 'error'));
    });
    menu.querySelector('.change-avatar-btn').addEventListener('click', () => {
        if (menu && menu.parentNode) {
            menu.parentNode.removeChild(menu);
        }
        openAvatarChangeModal(characterId, characterName);
    });

    const closeHandler = (e) => {
        if (menu && menu.parentNode && !menu.contains(e.target)) {
            menu.parentNode.removeChild(menu);
            document.removeEventListener('click', closeHandler);
        }
    };
    setTimeout(() => document.addEventListener('click', closeHandler), 0);
}