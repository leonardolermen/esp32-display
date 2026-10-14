# Spotify — design

- **Data:** 2026-10-14
- **Status:** aprovado em conversa
- **Detalha:** `2026-10-13-claudinho-design.md`, seção 7 (Spotify) e a pílula do Início (seção 6)

## 1. Objetivo

Uma página que mostra o que está tocando no Spotify do pai, com capa, progresso e controles (anterior, tocar/pausar,
próxima, volume), e uma pílula com a música atual no Início. O login acontece uma vez, pelo celular, sem client
secret e sem servidor próprio.

## 2. Onde entra

Ordem do deslize: **Início → Cripto → Spotify → Claude**. Código em `firmware/src/spotify/` (lógica pura na raiz,
`device/` para HTTPS e tarefa, `screen/` para LVGL, `web/` para rotas, `storage/` para NVS). O relay fica em
`relay/` na raiz do repositório.

## 3. App do Spotify

Reaproveita o app que já existe no projeto Modue. O Client ID `5ee610458f974597ba5c5f96eab539b5` é público e vem
pré-preenchido; a página deixa trocar. Escopos: `user-read-playback-state`, `user-modify-playback-state`,
`user-read-currently-playing`. Controles exigem Premium (o dono do app tem).

Passos manuais, uma vez:

1. No painel do Spotify (developer.spotify.com → app → Settings → Redirect URIs), acrescentar
   `https://leonardolermen.github.io/esp32-display/callback/`.
2. No GitHub (repositório → Settings → Pages), escolher "GitHub Actions" como fonte.
3. Para o pai usar com a conta dele enquanto o app está em modo de desenvolvimento: em Settings → User Management,
   acrescentar o e-mail da conta Spotify dele.

## 4. Login (PKCE com relay)

1. Na página `claudinho.local`, card "Spotify": Client ID e botão **Conectar**. A placa gera `code_verifier`
   (64 caracteres do alfabeto PKCE), `code_challenge` (SHA-256, base64url sem `=`) e um nonce de 16 bytes em hex,
   guarda os três na RAM por 10 min e devolve a URL de autorização.
2. `state` = `<ip>:<porta>.<nonce>` (ex.: `192.168.1.34:80.9f2c…`).
3. O navegador vai para `https://accounts.spotify.com/authorize?response_type=code&client_id=…&redirect_uri=…&
   code_challenge_method=S256&code_challenge=…&scope=…&state=…`.
4. O Spotify devolve para o relay com `code` e `state`. O relay (`relay/callback/index.html`, JS puro):
   - separa `ip:porta` do nonce;
   - aceita só IPv4 privado (10/8, 172.16/12, 192.168/16) e porta numérica;
   - redireciona para `http://<ip>:<porta>/api/spotify/callback?code=…&state=…`;
   - se o navegador barrar o salto de HTTPS para HTTP local, mostra a URL com botão **Copiar** e a instrução de
     colar no campo "Colar link" do card Spotify (plano B).
5. A placa confere o nonce (e a validade de 10 min), troca `code` + `code_verifier` em
   `POST https://accounts.spotify.com/api/token` e responde com uma página curta "Spotify conectado".
6. A tela da placa, enquanto desconectado, mostra o QR da mesma URL de autorização (gerada na hora) e "Escaneie para
   conectar o Spotify".

## 5. Tokens

- Access token só em RAM; refresh token na NVS (namespace `spotify`, chave `refresh`), junto do Client ID (`client`).
- Renovação 60 s antes de vencer, e uma vez em qualquer `401`.
- Se a renovação devolver um refresh token novo, ele substitui o antigo; se não devolver, o antigo fica.
- `invalid_grant` → apaga o refresh token e vai para o estado **desconectado**.
- Nenhuma rota devolve token. `DELETE /api/spotify` (autenticada) desconecta.

## 6. Dados e comandos

Tarefa própria no núcleo 0 (como o `MarketFeed`), HTTPS com o pacote de certificados do core. Estado publicado à UI
por `Published<PlayerState>`.

| Situação | Intervalo do `GET /v1/me/player` |
|---|---|
| Página Spotify ou Início visível, tocando | 3 s |
| Outra página visível, tocando | 15 s |
| Nada tocando, sem aparelho ou pausado há mais de 1 min | 30 s |
| Desconectado | não consulta |

A UI informa à tarefa qual página está visível por um `std::atomic`.

Respostas:

| Resposta | Estado |
|---|---|
| `200` com `item` | tocando ou pausado: título, artistas (separados por ", "), álbum, URL da maior capa, `id` do álbum, `progress_ms`, `duration_ms`, `is_playing`, nome do aparelho, `volume_percent` (ou ausente) |
| `200` com `item` nulo (anúncio, podcast sem metadados) | "tocando algo" sem título |
| `204` | sem aparelho ativo |
| `401` | renova e tenta uma vez |
| `429` | espera `Retry-After` (padrão 30 s) |
| outros / falha de rede | mantém o último estado, marca `stale` |

Comandos chegam da UI por uma fila FreeRTOS de 8 posições: tocar, pausar, próxima, anterior, volume. Cada um mexe no
estado local na hora (otimista: o ícone troca, o volume anda) e dispara um poll 600 ms depois para reconciliar. Volume
com debounce de 300 ms: só o último valor da janela é enviado. `403` no volume → o slider some para aquele aparelho até
o aparelho mudar. `404 NO_ACTIVE_DEVICE` → estado "sem aparelho".

## 7. Capa e cor

- Baixa a capa só quando o `id` do álbum muda; escolhe a imagem de ~300 px da lista (a mais próxima de 300).
- JPEGDEC decodifica para RGB565; a redução para **200×200** é feita uma vez, por média de blocos (área), num buffer
  na PSRAM. Lógica pura: `CoverScaler::downscale(src, srcW, srcH, dst, 200)`.
- Cor predominante: histograma de 4 bits por canal (4096 caixas) sobre pixels amostrados a cada 4, ignorando quase
  pretos e quase brancos; a caixa mais cheia vira a cor; depois escurece até a luminância relativa ficar ≤ 0,18
  (contraste com o texto `#F5F1EA` ≥ 4,5). Sem pixel útil → `theme::surface()`.
- O fundo da página faz transição de 400 ms da cor anterior para a nova.

## 8. Tela

Paisagem 480×320.

| Região | Conteúdo |
|---|---|
| Esquerda | capa 200×200 com cantos de 12 px, margem de 20 px |
| Direita, em cima | título (`fonts::large`, até 2 linhas, reticências), artistas (`fonts::medium`, 1 linha), aparelho (`fonts::small`, `theme::textSecondary`) |
| Direita, meio | barra de progresso 4 px com tempo decorrido e total (`m:ss`) |
| Direita, embaixo | anterior / tocar-pausar / próxima (ícones `LV_SYMBOL_*`, alvo de toque ≥ 48 px) e slider de volume |

O progresso avança sozinho entre polls quando `is_playing`, a partir de `progress_ms` e do instante da resposta, sem
passar de `duration_ms`.

Estados especiais, no lugar do conteúdo:

| Estado | Tela |
|---|---|
| Desconectado | QR de conectar + "Escaneie para conectar o Spotify" |
| Sem aparelho | ícone de música + "Abra o Spotify em algum aparelho" |
| `stale` há mais de 1 min | conteúdo normal + "sem conexão" pequeno no canto |

**Pílula no Início**: com algo tocando, uma cápsula no canto superior direito (altura 28 px, fundo preto a 50 %) com
o ícone de nota e "Título — Artista" em `fonts::small`, cortado com reticências a 220 px. Pausado ou sem aparelho,
some.

## 9. Código

```
relay/callback/index.html     página do relay (JS puro, sem dependência)
relay/callback/relay.js       parseState + isPrivateIpv4 + montagem da URL (testado com node --test)
.github/workflows/pages.yml   publica relay/ no GitHub Pages

spotify/
  Pkce.*            verifier, challenge (SHA-256 + base64url), nonce; SHA-256 próprio na lógica pura
  AuthRequest.*     URL de autorização, state "<ip>:<porta>.<nonce>", conferência do nonce com validade
  PlayerState.h     struct Track, enum PlayerStatus { Disconnected, NoDevice, Playing, Paused }, PlayerState
  PlayerParser.*    JSON do /me/player → PlayerState (ArduinoJson com filtro)
  TokenResponse.*   JSON do /api/token → access, refresh opcional, expires_in, erro
  PollSchedule.*    intervalo conforme visibilidade e estado
  Progress.*        progresso estimado entre polls e formato m:ss
  CoverScaler.*     redução por média de blocos
  DominantColor.*   histograma + escurecimento por luminância
  device/SpotifyClient.*   tarefa, HTTPS, tokens, fila de comandos, download da capa
  storage/SpotifyVault.*   NVS: client id e refresh token
  web/SpotifyRoutes.*      /api/spotify (status, conectar, colar link, desconectar) e /api/spotify/callback
  screen/SpotifyPage.*     a página
  screen/NowPlayingPill.*  a pílula do Início
```

`HomePage` recebe a pílula pronta (composição), sem conhecer o Spotify.

## 10. Erros

| Situação | O que acontece |
|---|---|
| Nonce errado ou vencido no callback | página "link vencido, tente de novo"; nada é gravado |
| Troca do código falha | página com o erro do Spotify; continua desconectado |
| Sem Wi-Fi | não consulta; estado anterior fica e vira `stale` |
| Capa não baixa ou não decodifica | capa cinza `theme::surface()`, cor de fundo padrão |
| PSRAM sem espaço para a capa | idem |
| Comando falha | o próximo poll desfaz o estado otimista |

## 11. Testes

No PC (`pio test -e native`):

- `test_pkce`: SHA-256 de "abc" e da string vazia (vetores do NIST); challenge do exemplo da RFC 7636
  (`dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk` → `E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM`); verifier com 64
  caracteres do alfabeto permitido.
- `test_auth_request`: URL com todos os parâmetros codificados; state montado e lido de volta; nonce errado e vencido
  recusados.
- `test_player_parser`: tocando, pausado, `item` nulo, sem `device.volume_percent`, vários artistas, escolha da capa
  de ~300 px, JSON quebrado.
- `test_token_response`: com e sem refresh token novo; `invalid_grant`.
- `test_poll_schedule`: as quatro linhas da tabela; `Retry-After`.
- `test_progress`: avança com o relógio quando tocando; para quando pausado; nunca passa da duração; `m:ss`.
- `test_cover_scaler`: 300×300 de uma cor → 200×200 da mesma cor; tabuleiro → cinza médio; 640×640 → 200×200.
- `test_dominant_color`: imagem vermelha → vermelho escurecido; preto e branco puros ignorados; luminância final
  ≤ 0,18.

No Node (`node --test`): `relay.test.mjs` com IPs privados aceitos, públicos e `localhost` recusados, state malformado,
porta não numérica.

Na placa (`docs/roteiro-de-teste.md`, seção "Spotify"): login pelo celular e pelo QR; plano B do link colado; música
aparece em até 3 s; controles respondem; volume; troca de aparelho; capa e cor mudam com o álbum; reinício mantém a
conexão; desconectar pela página.

## 12. Fora desta spec

Busca, playlists, fila, curtir música, letra, escolher aparelho pela tela.
