const status = await fetch('/api/status').then((response) => response.json());

document.getElementById('clock').textContent = status.time;
document.getElementById('photos').classList.toggle('hidden', !status.adminSet);

const form = document.getElementById('password-form');
const error = document.getElementById('password-error');

form.addEventListener('submit', async (event) => {
    event.preventDefault();
    error.textContent = '';

    const password = document.getElementById('password-value').value;
    const confirm = document.getElementById('password-confirm').value;
    if (password.trim().length < 8 || password.trim().length > 64) {
        error.textContent = 'De 8 a 64 caracteres.';
        return;
    }
    if (password !== confirm) {
        error.textContent = 'As duas senhas não batem.';
        return;
    }

    const response = await fetch('/api/admin/password', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ password }),
    });
    if (!response.ok) {
        const body = await response.json().catch(() => ({ error: 'não deu certo' }));
        error.textContent = body.error;
        return;
    }

    location.reload();
});
