import test from 'node:test';
import assert from 'node:assert/strict';

import { cropRect } from '../../web/crop.js';

test('a wide panorama is cropped to 3:2 centered horizontally', () => {
    assert.deepEqual(cropRect(3000, 1000), { x: 750, y: 0, width: 1500, height: 1000 });
});

test('a portrait keeps the crop centered at one third of the height, where faces are', () => {
    assert.deepEqual(cropRect(1000, 3000), { x: 0, y: 667, width: 1000, height: 667 });
});

test('a portrait crop never starts above the top', () => {
    assert.deepEqual(cropRect(1000, 700), { x: 0, y: 0, width: 1000, height: 667 });
});

test('an exact 3:2 image is untouched', () => {
    assert.deepEqual(cropRect(480, 320), { x: 0, y: 0, width: 480, height: 320 });
});
