import test from 'node:test';
import assert from 'node:assert/strict';

import { isPrivateIpv4, parseState, targetUrl } from '../callback/relay.js';

const NONCE = '0123456789abcdef0123456789abcdef';

test('private IPv4 ranges are accepted', () => {
    for (const host of ['10.0.0.5', '172.16.0.1', '172.31.255.254', '192.168.1.34']) {
        assert.ok(isPrivateIpv4(host), host);
    }
});

test('public, loopback and malformed hosts are refused', () => {
    for (const host of ['8.8.8.8', '172.32.0.1', '172.15.0.1', '127.0.0.1', 'localhost', '192.168.1', '192.168.1.256',
        'evil.com']) {
        assert.equal(isPrivateIpv4(host), false, host);
    }
});

test('a state with ip, port and nonce is parsed', () => {
    assert.deepEqual(parseState(`192.168.1.34:80.${NONCE}`), { host: '192.168.1.34', port: 80, nonce: NONCE });
});

test('malformed or public states are refused', () => {
    for (const state of ['', `192.168.1.34:http.${NONCE}`, '192.168.1.34:80.xyz', `8.8.8.8:80.${NONCE}`,
        `192.168.1.34:70000.${NONCE}`, null]) {
        assert.equal(parseState(state), null, String(state));
    }
});

test('the target sends code and state back to the board', () => {
    assert.equal(targetUrl(`?code=AQB-x&state=192.168.1.34%3A80.${NONCE}`),
        `http://192.168.1.34:80/api/spotify/callback?code=AQB-x&state=192.168.1.34%3A80.${NONCE}`);
});

test('no target without code or with a public host', () => {
    assert.equal(targetUrl(`?state=192.168.1.34%3A80.${NONCE}`), null);
    assert.equal(targetUrl(`?code=x&state=8.8.8.8%3A80.${NONCE}`), null);
});
