// O Spotify só devolve o login para um endereço HTTPS registrado; este relay recebe e manda o navegador para a placa
// na rede de casa. Só aceita IP privado para não virar um redirecionador aberto.
const STATE_PATTERN = /^([0-9.]+):([0-9]{1,5})\.([0-9a-f]{32})$/;
const MAX_PORT = 65535;

export function isPrivateIpv4(host) {
    const parts = String(host).split('.');
    const wellFormed = parts.length === 4 && parts.every((part) => /^[0-9]{1,3}$/.test(part) && Number(part) <= 255);
    if (!wellFormed) {
        return false;
    }

    const [first, second] = parts.map(Number);
    return first === 10 || (first === 172 && second >= 16 && second <= 31) || (first === 192 && second === 168);
}

export function parseState(state) {
    const match = STATE_PATTERN.exec(state ?? '');
    if (!match) {
        return null;
    }

    const [, host, portText, nonce] = match;
    const port = Number(portText);
    if (port < 1 || port > MAX_PORT || !isPrivateIpv4(host)) {
        return null;
    }

    return { host, port, nonce };
}

export function targetUrl(search) {
    const params = new URLSearchParams(search);
    const code = params.get('code');
    const state = params.get('state');
    const parsed = parseState(state);
    if (!code || !parsed) {
        return null;
    }

    return `http://${parsed.host}:${parsed.port}/api/spotify/callback?code=${encodeURIComponent(code)}`
        + `&state=${encodeURIComponent(state)}`;
}
