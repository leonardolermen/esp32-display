const RATIO = 3 / 2;
export const TARGET_WIDTH = 480;
export const TARGET_HEIGHT = 320;
const BACKGROUND_BLUR_PX = 12;
const BACKGROUND_DARKEN = 0.35;

// Recorte que cobre a tela inteira (3:2), usado no fundo desfocado.
export function cropRect(imageWidth, imageHeight) {
    if (imageWidth / imageHeight > RATIO) {
        const width = Math.round(imageHeight * RATIO);
        return { x: Math.round((imageWidth - width) / 2), y: 0, width, height: imageHeight };
    }

    const height = Math.round(imageWidth / RATIO);
    // Centro do recorte a um terço da altura: em foto em pé é onde ficam os rostos.
    const wanted = Math.round(imageHeight / 3 - height / 2);
    const y = Math.min(Math.max(wanted, 0), imageHeight - height);
    return { x: 0, y, width: imageWidth, height };
}

// Onde a foto inteira cabe na tela sem cortar nem distorcer: é o que aparece por cima do fundo.
export function fitRect(imageWidth, imageHeight) {
    const scale = Math.min(TARGET_WIDTH / imageWidth, TARGET_HEIGHT / imageHeight);
    const width = Math.round(imageWidth * scale);
    const height = Math.round(imageHeight * scale);
    return {
        x: Math.round((TARGET_WIDTH - width) / 2),
        y: Math.round((TARGET_HEIGHT - height) / 2),
        width,
        height,
    };
}

// A tela é 3:2 e as fotos de celular não são: em vez de dar zoom até cobrir (que corta e deixa feio), a foto inteira
// fica no meio e as sobras são a própria foto, desfocada e escurecida. Tudo é feito aqui, no navegador; a placa só
// recebe um JPEG já em 480×320.
export async function composeToJpeg(file) {
    const bitmap = await createImageBitmap(file);
    const cover = cropRect(bitmap.width, bitmap.height);
    const fit = fitRect(bitmap.width, bitmap.height);

    const canvas = document.createElement('canvas');
    canvas.width = TARGET_WIDTH;
    canvas.height = TARGET_HEIGHT;
    const context = canvas.getContext('2d');

    context.filter = `blur(${BACKGROUND_BLUR_PX}px)`;
    context.drawImage(bitmap, cover.x, cover.y, cover.width, cover.height, 0, 0, TARGET_WIDTH, TARGET_HEIGHT);
    context.filter = 'none';
    context.fillStyle = `rgba(0, 0, 0, ${BACKGROUND_DARKEN})`;
    context.fillRect(0, 0, TARGET_WIDTH, TARGET_HEIGHT);

    context.drawImage(bitmap, 0, 0, bitmap.width, bitmap.height, fit.x, fit.y, fit.width, fit.height);

    return new Promise((resolve) => canvas.toBlob(resolve, 'image/jpeg', 0.9));
}
