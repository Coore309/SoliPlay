// Copyright (C) 2026 Coore309
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

document.getElementById('start-create-btn').addEventListener('click', () => {
    showView('create');
    sidebar.classList.remove('open');
});
document.getElementById('new-story-btn').addEventListener('click', () => {
    showView('create');
    sidebar.classList.remove('open');
});

document.getElementById('back-btn').addEventListener('click', () => {
    showView(document.getElementById('chat-messages').children.length > 0 ? 'chat' : 'start');
});
document.getElementById('create-cancel').addEventListener('click', () => {
    showView(document.getElementById('chat-messages').children.length > 0 ? 'chat' : 'start');
});

// ========== 设计页面逻辑 ==========
let advancedOpen = false;
document.getElementById('advanced-toggle').addEventListener('click', () => {
    advancedOpen = !advancedOpen;
    document.getElementById('advanced-options').style.display = advancedOpen ? 'block' : 'none';
    document.getElementById('advanced-toggle').innerHTML = advancedOpen ? '⚙️ 高级选项 ▴' : '⚙️ 高级选项 ▾';
});

let currentMode = 'lightnovel';

function updateProtagonistCard() {
    const hint = document.getElementById('protagonist-card-hint');
    const attrs = ['str', 'dex', 'int', 'con', 'cha'];
    if (currentMode === 'trpg') {
        hint.textContent = '（用于判定，属性范围1-20，技能0-100）';
        attrs.forEach(a => {
            const input = document.getElementById('attr-' + a);
            input.value = 10;
            input.min = 1;
            input.max = 20;
        });
    } else {
        hint.textContent = '（仅作背景参考，无上限，数值体现角色特点）';
        attrs.forEach(a => {
            const input = document.getElementById('attr-' + a);
            input.value = 50;
            input.min = 1;
            input.max = 200;
        });
    }
}
document.querySelectorAll('.mode-card').forEach(card => {
    card.addEventListener('click', () => {
        document.querySelectorAll('.mode-card').forEach(c => c.classList.remove('selected'));
        card.classList.add('selected');
        currentMode = card.dataset.mode;
        document.getElementById('novel-sub-options').style.display = currentMode === 'lightnovel' ? 'block' : 'none';
        updateProtagonistCard();
    });
});
updateProtagonistCard();

let novelStyle = 'none',
    audience = 'none',
    focus = 'none';
document.querySelectorAll('.sub-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const parent = btn.parentElement;
        parent.querySelectorAll('.sub-btn').forEach(b => b.classList.remove('selected'));
        btn.classList.add('selected');
        if (btn.dataset.sub) novelStyle = btn.dataset.sub;
        if (btn.dataset.aud) audience = btn.dataset.aud;
        if (btn.dataset.focus) focus = btn.dataset.focus;
    });
});

document.getElementById('story-randomness').addEventListener('input', e => document.getElementById('story-randomness-value').textContent = e.target.value + '%');
document.getElementById('character-randomness').addEventListener('input', e => document.getElementById('character-randomness-value').textContent = e.target.value + '%');

// ========== 复活故事逻辑 ==========
let resurrectCharacters = [];
let resurrectCharCounter = 0;

document.getElementById('enable-resurrect').addEventListener('change', (e) => {
    document.getElementById('resurrect-options').style.display = e.target.checked ? 'block' : 'none';
});

document.getElementById('add-resurrect-char-btn').addEventListener('click', () => {
    resurrectCharCounter++;
    const charId = `resurrect_char_${Date.now()}`;
    const charObj = {
        id: charId,
        name: `角色 ${String(resurrectCharCounter).padStart(2, '0')}`,
        gender: '',
        appearance: '',
        background: '',
        storyExperience: '',
        relationToMain: '',
        relationToOthers: '',
        currentFeeling: '',
        speechStyle: '',
        inventory: '',
        attributes: '',
        skills: '',
        specialAbility: ''
    };
    resurrectCharacters.push(charObj);
    renderResurrectCharList();
});

// 批量导入按钮触发文件选择
document.getElementById('import-char-btn').addEventListener('click', () => {
    document.getElementById('import-char-file').click();
});

// 多文件导入逻辑
document.getElementById('import-char-file').addEventListener('change', (e) => {
    const files = e.target.files;
    if (!files.length) {
        showToast('未选择任何文件', 'info');
        return;
    }

    let importedCount = 0;
    let total = files.length;

    for (const file of files) {
        const reader = new FileReader();
        reader.onload = (event) => {
            try {
                const char = parseCharFile(event.target.result);
                resurrectCharCounter++;
                char.id = `resurrect_char_${Date.now()}_${importedCount}`;
                resurrectCharacters.push(char);
                importedCount++;
            } catch (err) {
                console.error(`解析文件 ${file.name} 失败：`, err);
                showToast(`文件 ${file.name} 格式错误，已跳过`, 'error');
            }
            if (importedCount === total) {
                renderResurrectCharList();
                if (importedCount > 0) {
                    showToast(`已成功导入 ${importedCount} 个角色`, 'success');
                }
            }
        };
        reader.onerror = () => {
            showToast(`读取文件 ${file.name} 失败`, 'error');
            total--;
            if (importedCount === total) {
                renderResurrectCharList();
                if (importedCount > 0) {
                    showToast(`已成功导入 ${importedCount} 个角色`, 'success');
                }
            }
        };
        reader.readAsText(file, 'UTF-8');
    }
    e.target.value = '';
});

function parseCharFile(content) {
    const char = {
        id: '',
        name: '',
        gender: '',
        appearance: '',
        background: '',
        storyExperience: '',
        relationToMain: '',
        relationToOthers: '',
        currentFeeling: '',
        speechStyle: '',
        inventory: '',
        attributes: '',
        skills: '',
        specialAbility: ''
    };

    const lines = content.split('\n');
    for (const line of lines) {
        let colonIndex = line.indexOf('：');
        if (colonIndex === -1) colonIndex = line.indexOf(':');
        if (colonIndex === -1) continue;
        const label = line.substring(0, colonIndex).trim();
        const value = line.substring(colonIndex + 1).trim();

        if (label === '角色名') char.name = value;
        else if (label === '性别') char.gender = value;
        else if (label === '角色形象') char.appearance = value;
        else if (label === '背景') char.background = value;
        else if (label === '在 story 中的经历') char.storyExperience = value;
        else if (label.startsWith('对主角（') || label.startsWith('对主角(')) char.relationToMain = value;
        else if (label === '当下对主角的感受') char.currentFeeling = value;
        else if (label === '对其他人的关系、态度的整个历程') char.relationToOthers = value;
        else if (label === '说话特点') char.speechStyle = value;
        else if (label === '物品') char.inventory = value;
        else if (label === '能力数值') char.attributes = value;
        else if (label === '技能') char.skills = value;
        else if (label === '特殊能力') char.specialAbility = value;
    }

    if (!char.name) char.name = `角色 ${String(resurrectCharCounter + 1).padStart(2, '0')}`;
    return char;
}

function renderResurrectCharList() {
    const listDiv = document.getElementById('resurrect-char-list');
    const countSpan = document.getElementById('resurrect-char-count');
    countSpan.textContent = resurrectCharacters.length;
    listDiv.innerHTML = '';
    resurrectCharacters.forEach((char, index) => {
        const item = document.createElement('div');
        item.className = 'resurrect-char-item';

        const charBtn = document.createElement('button');
        charBtn.className = 'btn-text';
        charBtn.textContent = char.name;
        charBtn.addEventListener('click', () => {
            showResurrectCharModal(index);
        });
        item.appendChild(charBtn);

        const deleteBtn = document.createElement('button');
        deleteBtn.className = 'btn-text';
        deleteBtn.style.color = '#c62828';
        deleteBtn.textContent = '×';
        deleteBtn.addEventListener('click', () => {
            showConfirmModal(`确定删除「${char.name}」吗？`, (confirmed) => {
                if (confirmed) {
                    resurrectCharacters.splice(index, 1);
                    renderResurrectCharList();
                }
            });
        });
        item.appendChild(deleteBtn);

        listDiv.appendChild(item);
    });
}

function showResurrectCharModal(index) {
    const char = resurrectCharacters[index];
    const protagonistName = document.getElementById('protagonist-name').value.trim() || '主角';

    const modal = document.createElement('div');
    modal.className = 'modal active';
    modal.innerHTML = `
        <div class="modal-card wide">
            <div class="modal-header">
                <h3>编辑复活角色</h3>
                <button class="btn-text close-modal">✕</button>
            </div>
            <div class="modal-body">
                <div class="resurrect-form-grid">
                    <div>
                        <label class="input-label">角色名</label>
                        <input type="text" id="char-name-input" value="${char.name}">
                    </div>
                    <div>
                        <label class="input-label">性别</label>
                        <input type="text" id="char-gender-input" value="${char.gender}">
                    </div>
                    <div>
                        <label class="input-label">角色形象</label>
                        <textarea id="char-appearance-input" rows="3">${char.appearance}</textarea>
                    </div>
                    <div>
                        <label class="input-label">背景</label>
                        <textarea id="char-background-input" rows="3">${char.background}</textarea>
                    </div>
                    <div>
                        <label class="input-label">在 story 中的经历</label>
                        <textarea id="char-story-experience-input" rows="3">${char.storyExperience}</textarea>
                    </div>
                    <div>
                        <label class="input-label">说话特点</label>
                        <textarea id="char-speech-style-input" rows="3">${char.speechStyle}</textarea>
                    </div>
                    <div>
                        <label class="input-label">对主角（${protagonistName}）的关系、态度的整个历程</label>
                        <textarea id="char-relation-main-input" rows="3">${char.relationToMain}</textarea>
                    </div>
                    <div>
                        <label class="input-label">当下对主角的感受</label>
                        <textarea id="char-current-feeling-input" rows="3">${char.currentFeeling}</textarea>
                    </div>
                    <div class="full-width">
                        <label class="input-label">对其他人的关系、态度的整个历程</label>
                        <textarea id="char-relation-others-input" rows="3">${char.relationToOthers}</textarea>
                    </div>
                    <div>
                        <label class="input-label">物品</label>
                        <textarea id="char-inventory-input" rows="2">${char.inventory}</textarea>
                    </div>
                    <div>
                        <label class="input-label">能力数值</label>
                        <textarea id="char-attributes-input" rows="2">${char.attributes}</textarea>
                    </div>
                    <div>
                        <label class="input-label">技能</label>
                        <textarea id="char-skills-input" rows="2">${char.skills}</textarea>
                    </div>
                    <div>
                        <label class="input-label">特殊能力</label>
                        <textarea id="char-special-ability-input" rows="2">${char.specialAbility}</textarea>
                    </div>
                </div>
            </div>
            <div class="create-actions">
                <button class="btn-text cancel-modal">取消</button>
                <button class="btn-primary save-char-btn">保存</button>
            </div>
        </div>
    `;
    document.body.appendChild(modal);

    const closeModal = () => modal.remove();
    modal.querySelector('.close-modal').addEventListener('click', closeModal);
    modal.querySelector('.cancel-modal').addEventListener('click', closeModal);
    modal.addEventListener('click', (e) => { if (e.target === modal) closeModal(); });

    modal.querySelector('.save-char-btn').addEventListener('click', () => {
        char.name = modal.querySelector('#char-name-input').value.trim() || `角色 ${String(index + 1).padStart(2, '0')}`;
        char.gender = modal.querySelector('#char-gender-input').value.trim();
        char.appearance = modal.querySelector('#char-appearance-input').value;
        char.background = modal.querySelector('#char-background-input').value;
        char.storyExperience = modal.querySelector('#char-story-experience-input').value;
        char.speechStyle = modal.querySelector('#char-speech-style-input').value;
        char.relationToMain = modal.querySelector('#char-relation-main-input').value;
        char.currentFeeling = modal.querySelector('#char-current-feeling-input').value;
        char.relationToOthers = modal.querySelector('#char-relation-others-input').value;
        char.inventory = modal.querySelector('#char-inventory-input').value;
        char.attributes = modal.querySelector('#char-attributes-input').value;
        char.skills = modal.querySelector('#char-skills-input').value;
        char.specialAbility = modal.querySelector('#char-special-ability-input').value;
        renderResurrectCharList();
        closeModal();
    });
}

// ========== 开始冒险（兼容复活故事，收集转述者开关） ==========
document.getElementById('create-confirm').addEventListener('click', () => {
    const requirements = document.getElementById('story-requirements').value.trim();
    const isResurrect = document.getElementById('enable-resurrect').checked;
    const enableNarrator = document.getElementById('enable-narrator').checked; // 转述者开关

    if (isResurrect) {
        const storyWorldview = document.getElementById('story-worldview').value.trim();
        const storyChain = document.getElementById('story-chain').value.trim();
        const plotStyle = document.getElementById('plot-style').value.trim();
        if (!storyChain) {
            showToast('请填写故事链', 'info');
            return;
        }
        const extra = {
            mode: currentMode,
            storyRandomness: parseInt(document.getElementById('story-randomness').value),
            characterRandomness: parseInt(document.getElementById('character-randomness').value),
            useProForActors: document.getElementById('use-pro-actors').checked,
            enable_narrator: enableNarrator,
            resurrect: true,
            story_worldview: storyWorldview,
            story_chain: storyChain,
            plot_style: plotStyle,
            characters: resurrectCharacters,
            protagonist: {
                name: document.getElementById('protagonist-name').value.trim() || '未知',
                background: document.getElementById('protagonist-background').value.trim(),
                attributes: {
                    str: parseInt(document.getElementById('attr-str').value) || 10,
                    dex: parseInt(document.getElementById('attr-dex').value) || 10,
                    int: parseInt(document.getElementById('attr-int').value) || 10,
                    con: parseInt(document.getElementById('attr-con').value) || 10,
                    cha: parseInt(document.getElementById('attr-cha').value) || 10
                },
                skills: {},
                specialAbility: document.getElementById('special-ability-input').value.trim(),
                inventory: []
            }
        };
        const skillsStr = document.getElementById('skills-input').value.trim();
        if (skillsStr) { skillsStr.split(',').forEach(s => { const parts = s.trim().split(/(\d+)/); if (parts.length >= 2) extra.protagonist.skills[parts[0].trim()] = parseInt(parts[1]); }); }
        const invStr = document.getElementById('inventory-input').value.trim();
        if (invStr) extra.protagonist.inventory = invStr.split(',').map(s => s.trim());

        document.getElementById('input-prompt').textContent = '';
        userInput.value = '';
        userInput.disabled = true;
        sendBtn.disabled = true;
        showLoading();
        showView('chat');
        sendTurn('start_story', requirements, extra);
    } else {
        if (!requirements) return;
        const extra = {
            mode: currentMode,
            storyRandomness: parseInt(document.getElementById('story-randomness').value),
            characterRandomness: parseInt(document.getElementById('character-randomness').value),
            useProForActors: document.getElementById('use-pro-actors').checked,
            enable_narrator: enableNarrator
        };
        if (advancedOpen && currentMode === 'lightnovel') {
            extra.novelStyle = novelStyle;
            extra.audience = audience;
            extra.focus = focus;
        }
        const taboos = document.getElementById('taboos-input').value.split(',').map(t => t.trim()).filter(t => t);
        extra.taboos = taboos;
        extra.protagonist = {
            name: document.getElementById('protagonist-name').value.trim() || '未知',
            background: document.getElementById('protagonist-background').value.trim(),
            attributes: {
                str: parseInt(document.getElementById('attr-str').value) || 10,
                dex: parseInt(document.getElementById('attr-dex').value) || 10,
                int: parseInt(document.getElementById('attr-int').value) || 10,
                con: parseInt(document.getElementById('attr-con').value) || 10,
                cha: parseInt(document.getElementById('attr-cha').value) || 10
            },
            skills: {},
            specialAbility: document.getElementById('special-ability-input').value.trim(),
            inventory: []
        };
        const skillsStr = document.getElementById('skills-input').value.trim();
        if (skillsStr) { skillsStr.split(',').forEach(s => { const parts = s.trim().split(/(\d+)/); if (parts.length >= 2) extra.protagonist.skills[parts[0].trim()] = parseInt(parts[1]); }); }
        const invStr = document.getElementById('inventory-input').value.trim();
        if (invStr) extra.protagonist.inventory = invStr.split(',').map(s => s.trim());

        document.getElementById('input-prompt').textContent = '';
        userInput.value = '';
        userInput.disabled = true;
        sendBtn.disabled = true;
        showLoading();
        showView('chat');
        sendTurn('start_story', requirements, extra);
    }
});