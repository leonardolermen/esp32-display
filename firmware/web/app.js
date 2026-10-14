import { composeToJpeg } from '/crop.js';

const status = await fetch('/api/status').then((response) => response.json());

document.getElementById('clock').textContent = status.time;
document.getElementById('photos').classList.toggle('hidden', !status.adminSet);
document.getElementById('birthday').classList.toggle('hidden', !status.adminSet);

if (status.adminSet) {
    document.getElementById('pin-title').textContent = 'Trocar o PIN';
    document.getElementById('pin-hint').textContent = 'Já existe um PIN. Digite o novo, de 4 a 6 números.';
    document.getElementById('pin-submit').textContent = 'Trocar PIN';
}

const form = document.getElementById('pin-form');
const error = document.getElementById('pin-error');

form.addEventListener('submit', async (event) => {
    event.preventDefault();
    error.textContent = '';

    const pin = document.getElementById('pin-value').value.trim();
    const confirm = document.getElementById('pin-confirm').value.trim();
    if (!/^[0-9]{4,6}$/.test(pin)) {
        error.textContent = 'De 4 a 6 números.';
        return;
    }
    if (pin !== confirm) {
        error.textContent = 'Os dois PINs não batem.';
        return;
    }

    const response = await fetch('/api/admin/pin', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ pin }),
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

        const jpeg = await composeToJpeg(file);
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

document.getElementById('birthday-replay').addEventListener('click', async () => {
    const response = await fetch('/api/birthday/replay', { method: 'POST' });
    document.getElementById('birthday-status').textContent = response.ok ? 'olha a tela' : 'não deu certo';
});

if (status.adminSet) {
    refreshPhotos();
}
