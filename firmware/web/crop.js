const RATIO = 3 / 2;
export const TARGET_WIDTH = 480;
export const TARGET_HEIGHT = 320;

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

export async function cropToJpeg(file) {
    const bitmap = await createImageBitmap(file);
    const rect = cropRect(bitmap.width, bitmap.height);

    const canvas = document.createElement('canvas');
    canvas.width = TARGET_WIDTH;
    canvas.height = TARGET_HEIGHT;
    canvas.getContext('2d').drawImage(bitmap, rect.x, rect.y, rect.width, rect.height, 0, 0, TARGET_WIDTH, TARGET_HEIGHT);

    return new Promise((resolve) => canvas.toBlob(resolve, 'image/jpeg', 0.85));
}
