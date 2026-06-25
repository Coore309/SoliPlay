const startView = document.getElementById('start-view');
const chatView = document.getElementById('chat-view');
const createView = document.getElementById('create-view');
const settingsView = document.getElementById('settings-view');
const avatarView = document.getElementById('avatar-view');
const views = [startView, chatView, createView, settingsView, avatarView];

function showView(viewName) {
    views.forEach(v => v.classList.remove('active'));
    if (viewName === 'start') startView.classList.add('active');
    else if (viewName === 'chat') chatView.classList.add('active');
    else if (viewName === 'create') createView.classList.add('active');
    else if (viewName === 'settings') {
        settingsView.classList.add('active');
        if (typeof loadSettings === 'function') loadSettings();
    } else if (viewName === 'avatar') {
        avatarView.classList.add('active');
        if (typeof initAvatarView === 'function') initAvatarView();
    }
}