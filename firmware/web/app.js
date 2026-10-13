import { cropToJpeg } from '/crop.js';

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

const grid = document.getElementById('photo-grid');
const progress = document.getElementById('photo-progress');

async function refreshPhotos() {
    const photos = await fetch('/api/photos').then((response) => response.json());

    grid.replaceChildren(...photos.map((photo) => {
        const figure = document.createElement('figure');
        const image = document.createElement('img');
        image.src = `/photos/${photo.name}`;
        image.alt = '';
        const remove = document.createElement('button');
        remove.textContent = 'apagar';
        remove.addEventListener('click', async () => {
            if (!confirm('Apagar esta foto?')) {
                return;
            }
            await fetch(`/api/photos/${photo.name}`, { method: 'DELETE' });
            refreshPhotos();
        });
        figure.append(image, remove);
        return figure;
    }));
}

document.getElementById('photo-input').addEventListener('change', async (event) => {
    const files = [...event.target.files];

    for (const [position, file] of files.entries()) {
        progress.textContent = `enviando ${position + 1} de ${files.length}…`;

        const jpeg = await cropToJpeg(file);
        const body = new FormData();
        body.append('photo', jpeg, 'photo.jpg');
        const response = await fetch('/api/photos', { method: 'POST', body });
        if (!response.ok) {
            const error = await response.json().catch(() => ({ error: 'não deu certo' }));
            progress.textContent = `${file.name}: ${error.error}`;
            return;
        }
    }

    progress.textContent = files.length ? 'pronto' : '';
    event.target.value = '';
    refreshPhotos();
});

if (status.adminSet) {
    refreshPhotos();
}
