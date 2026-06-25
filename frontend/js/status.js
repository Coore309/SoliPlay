const statusModal = document.getElementById('status-modal');
const statusContent = document.getElementById('status-content');
document.getElementById('status-btn').addEventListener('click', async() => {
    try {
        const res = await fetch('/api/status');
        const data = await res.json();
        let html = '<div class="status-grid"><div><b>属性</b>';
        for (const [key, val] of Object.entries(data.attributes || {})) html += `<div>${key}: ${val}</div>`;
        html += '</div><div><b>技能</b>';
        const skills = data.skills || {};
        if (Object.keys(skills).length === 0) html += '<div>无</div>';
        else
            for (const [key, val] of Object.entries(skills)) html += `<div>${key}: ${val}</div>`;
        html += '</div></div>';
        html += '<div style="margin-top:12px;"><b>特殊能力</b></div><div>' + (data.specialAbility || '无') + '</div>';
        html += '<div style="margin-top:8px;"><b>物品</b></div><div>' + ((data.inventory || []).length ? data.inventory.join(', ') : '无') + '</div>';
        statusContent.innerHTML = html;
        statusModal.classList.add('active');
    } catch (err) {
        console.error('获取状态失败', err);
        showToast('获取主角状态失败', 'error');
    }
});
document.getElementById('status-close').addEventListener('click', () => statusModal.classList.remove('active'));
statusModal.addEventListener('click', e => { if (e.target === statusModal) statusModal.classList.remove('active'); });