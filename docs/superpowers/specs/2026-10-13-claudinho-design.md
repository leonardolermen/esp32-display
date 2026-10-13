# Claudinho — design

- **Data:** 2026-10-13
- **Prazo:** 2026-10-22 (aniversário)
- **Status:** aguardando revisão
- **Nome:** "Claudinho" é provisório; vira o endereço `claudinho.local` e aparece na tela de boas-vindas.

## 1. Objetivo

Presente de aniversário: um aparelho de mesa feito com a placa Guition JC3248W535C que mostra fotos da família,
controla o Spotify e acompanha o uso do Claude Code do pai. Ele é dev (Go, Kotlin, Java; gosta mais de Go) e o
projeto precisa ser algo que ele consiga abrir, entender e estender.

### Critérios de sucesso

- Ligou pela primeira vez, a mensagem de aniversário aparece — sem depender de rede.
- A configuração inicial é guiada pela própria tela e leva menos de 10 minutos, sem instalar nada no Mac.
- Fotos, relógio e Spotify funcionam com o Mac desligado.
- A página do Claude se atualiza enquanto ele usa o Claude Code, e o alerta de permissão aparece em menos de 2 s.
- Seguindo o guia do repositório, ele cria uma página nova sem precisar entender o resto do firmware.

### Restrições

- **Nada instalado no Mac.** Só linhas de configuração no `~/.claude/settings.json`, usando o que o macOS já tem (`curl`).
- O pai tem Spotify Premium; o app de desenvolvedor do Spotify fica na conta dele.
- 9 dias de prazo: escopo priorizado (seção 15), cortado de baixo para cima se faltar tempo.

## 2. Hardware

| Item | Valor |
|---|---|
| SoC | ESP32-S3 N16R8 — 16 MB flash, 8 MB PSRAM, Wi-Fi + BLE |
| Tela | IPS 3,5" 320×480, controlador AXS15231B em QSPI |
| Touch | capacitivo, integrado ao AXS15231B, I2C endereço `0x3B` |
| Pinos (exemplo F1ATB) | QSPI: CS 45, SCK 47, D0 21, D1 48, D2 40, D3 39 · backlight 1 · touch SDA 4, SCL 8 |
| Também tem | slot microSD, saída de som I2S, conector de bateria — **fora da v1** |

Limitações conhecidas do AXS15231B, que moldam o firmware:

- Só funciona com o LVGL em **full refresh** (atualização parcial quebra a imagem).
- **Sem rotação por hardware**: a paisagem 480×320 é rotação por software.
- Arduino_GFX **1.6.0** funciona; a 1.6.1 é relatada como quebrada com esta placa.
- Reset e interrupção do touch e os pinos do microSD aparecem em conflito nas fontes — confirmar no esquemático
  antes de usar qualquer um dos dois.

## 3. Arquitetura

Tudo roda na placa. O Mac só manda dados por configuração do próprio Claude Code.

```
Mac do pai                                   Claudinho (ESP32-S3)                         Internet
──────────                                   ────────────────────                         ────────
Claude Code ── barra de status (curl) ──▶ POST /api/claude/statusline ┐
            ── hooks (curl assíncrono) ─▶ POST /api/claude/hook       ├─▶ claude/  ─┐
            ── telemetria OTLP/JSON ────▶ POST /v1/metrics            ┘             │
                                                                                     ├─▶ estado publicado ─▶ tarefa de UI
Navegador ──── http://claudinho.local ──▶ setup/ (config, fotos, OTA)                │       (LVGL)
                                          spotify/ ◀──────── HTTPS ──────────────────┼──── api.spotify.com
                                          photos/  (LittleFS)                       ─┘
```

### Duas tarefas, uma regra

- **Tarefa de UI** (núcleo 1): a única que toca no LVGL. A cada tick lê o estado publicado e redesenha o que mudou.
- **Tarefa de rede** (núcleo 0): servidor web, Spotify, recepção dos dados do Claude.
- **Regra:** a rede nunca chama o LVGL. Cada conceito publica um snapshot (struct copiada, com número de versão,
  protegida por mutex); a UI compara a versão e só redesenha se mudou. Comandos da UI para a rede ("pausar",
  "volume 40") vão por uma fila.

## 4. Stack

| Peça | Escolha |
|---|---|
| Build | PlatformIO, plataforma pioarduino (Arduino core 3.x) — confirmar no teste mínimo |
| Interface | LVGL 9 (versão fixada), Arduino_GFX 1.6.0 como driver |
| Servidor web | ESPAsyncWebServer (fork mantido), corpo grande processado em pedaços |
| JSON | ArduinoJson 7 |
| JPEG | JPEGDEC (fotos e capas de álbum) |
| Persistência | LittleFS (fotos, config, histórico) + NVS (segredos: refresh token, senha) |
| Rede local | mDNS `claudinho.local` |
| Fontes | Inter via `lv_font_conv`, Latin-1 (acentos), 12/14/18/24 px + 64 px só dígitos e `:` |

Tabela de partições (16 MB): `nvs` 20 KB · `otadata` 8 KB · `app0` 3 MB · `app1` 3 MB · `littlefs` ~9,8 MB ·
`coredump` 64 KB.

## 5. Repositório

```
claudinho/
  firmware/
    platformio.ini
    partitions.csv
    src/
      main.cpp            liga as peças e registra as páginas
      board/              tela, touch, backlight — a fronteira com o hardware
      navigation/         registro de páginas, deslizar, indicador, overlay de alerta, modo noite
      photos/
      spotify/
      claude/
      birthday/
      setup/              Wi-Fi, portal cativo, página de configuração, OTA
      support/            log, armazenamento, relógio
    data/                 página web de configuração + birthday.json (vão para o LittleFS)
    test/                 espelha src/, roda no PC
  relay/                  página estática do login do Spotify (GitHub Pages)
  docs/
    como-criar-uma-pagina.md
    roteiro-de-teste.md
  CLAUDE.md               regras do projeto (C++/firmware) sobre o padrão global
```

### Páginas como módulos

```cpp
class Page {
public:
    virtual ~Page() = default;

    virtual const char* name() const = 0;
    virtual void build(lv_obj_t* screen) = 0;
    virtual void onShow() {}
    virtual void onHide() {}
    virtual void onTick(uint32_t nowMillis) {}
};
```

`build` cria os widgets uma única vez; `onTick` lê o estado publicado e atualiza só o que mudou. Uma página nova é
uma pasta nova e uma linha em `main.cpp`: `pages.add(std::make_unique<WeatherPage>(weather));`.

### Padrão de código

Segue o padrão global (nomes por extenso, pastas por conceito, chaves sempre, 120 colunas, value objects que validam
no construtor, sem cadeia de `if`, `switch` sobre enum em um lugar só, testes espelhando `src/`). O `CLAUDE.md` do
repositório acrescenta só o que é específico de C++ e firmware.

## 6. Telas

Paisagem 480×320, tema escuro. Cores: fundo `#1F1E1C`, texto `#F5F1EA`, secundário `#A8A29A`, destaque laranja
Claude `#D97757`, âmbar `#E8B04A`.

| Tela | Conteúdo | Comportamento |
|---|---|---|
| Início | foto em tela cheia, relógio e data, pílula com a música atual | fotos trocam com fade; relógio escondido até sincronizar a hora |
| Spotify | capa, título, artista, progresso, anterior/pausar/próxima, volume, aparelho | fundo assume a cor predominante da capa, com transição; controles respondem na hora |
| Claude | anéis de 5 h e semana com "zera às…", tokens e custo de hoje, barras por modelo, sessão em destaque, chip de atividade | anéis animam ao mudar |
| Alerta (overlay) | "Claude precisa de você", o que ele quer fazer, projeto | por cima de qualquer tela; toque dispensa |
| Terminou (toast) | "Claude terminou em <projeto>" | discreto, some em 5 s |
| Aniversário | mensagem, confete | na primeira vez que liga e na primeira vez que a tela acorda em todo 22/10 |
| Configuração | QR do Wi-Fi, depois QR de `claudinho.local`, passos | só enquanto não está configurado |

Navegação: deslizar para os lados entre Início → Spotify → Claude, com pontinhos embaixo. Parado 2 min fora do Início,
volta sozinho. Modo noite das 23 h às 7 h (configurável): brilho em 10 %, toque acorda por 30 s.

## 7. Spotify

### Login (PKCE, sem client secret)

1. O pai cria um app em developer.spotify.com (a página de configuração tem o passo a passo) e registra a redirect URI
   do relay: `https://<conta>.github.io/claudinho/callback`.
2. Cola o Client ID em `claudinho.local`. A placa gera `code_verifier`/`code_challenge` e um nonce, e mostra o QR da
   URL de autorização. Escopos: `user-read-playback-state`, `user-modify-playback-state`,
   `user-read-currently-playing`.
3. Ele escaneia, faz login no celular. O Spotify redireciona para o relay com `code` e `state`.
4. O `state` carrega `ip:porta` da placa e o nonce. O relay valida que o IP é privado (10/8, 172.16/12, 192.168/16) e
   redireciona o navegador para `http://<ip>/api/spotify/callback?code=…&state=…`.
5. A placa confere o nonce e troca `code` + `code_verifier` por tokens.

Plano B: se o navegador barrar o salto de HTTPS para HTTP local, o relay mostra a URL com um botão de copiar, e a página
de configuração aceita colar essa URL.

### Tokens

Access token só em RAM; refresh token na NVS. Renovação automática 60 s antes de expirar. Renovação recusada
(`invalid_grant` — revogado, ou vencido; há relato não oficial de validade de ~6 meses) → estado *desconectado* → a tela
do Spotify vira um QR de "reconectar". O refresh token nunca sai da placa por nenhuma API.

### Polling e comandos

- `GET /v1/me/player`: a cada 3 s com Spotify ou Início visível e algo tocando; 15 s nas outras telas; 30 s sem nada
  tocando.
- `429`: respeita `Retry-After`; se o motivo for `QUOTA_EXCEEDED`, recua para 60 s e mostra um aviso discreto.
- Comandos: `PUT play`/`pause`, `POST next`/`previous`, `PUT volume?volume_percent=`. A tela muda na hora
  (otimista) e reconcilia no próximo polling. Volume com debounce de 300 ms.
- `404 NO_ACTIVE_DEVICE` → "abra o Spotify em algum aparelho". `403` no volume (aparelho não permite) → volume
  escondido para aquele aparelho.

### Capa

Baixa a imagem de ~300 px só quando o álbum muda; decodifica com JPEGDEC e redimensiona **uma vez** para 200×200 (em
full refresh, escalar no desenho custaria em todo quadro). A cor predominante sai de um histograma quantizado da capa,
escurecida até garantir contraste com texto branco.

## 8. Claude

### O que vai no `~/.claude/settings.json` do Mac

A página `claudinho.local` gera o bloco com o endereço preenchido; ele cola. Esboço:

```json
{
  "statusLine": {
    "type": "command",
    "command": "input=$(cat); (printf '%s' \"$input\" | curl -s -m 2 -H 'Content-Type: application/json' --data-binary @- http://claudinho.local/api/claude/statusline >/dev/null 2>&1 &)"
  },
  "hooks": {
    "UserPromptSubmit": [{ "hooks": [{ "type": "command", "async": true, "command": "curl -s -m 2 -H 'Content-Type: application/json' --data-binary @- http://claudinho.local/api/claude/hook; exit 0" }] }],
    "Notification":     [{ "hooks": [{ "type": "command", "async": true, "command": "…mesmo curl…" }] }],
    "Stop":             [{ "hooks": [{ "type": "command", "async": true, "command": "…mesmo curl…" }] }],
    "SessionEnd":       [{ "hooks": [{ "type": "command", "async": true, "command": "…mesmo curl…" }] }]
  },
  "env": {
    "CLAUDE_CODE_ENABLE_TELEMETRY": "1",
    "OTEL_METRICS_EXPORTER": "otlp",
    "OTEL_EXPORTER_OTLP_PROTOCOL": "http/json",
    "OTEL_EXPORTER_OTLP_METRICS_ENDPOINT": "http://claudinho.local/v1/metrics",
    "OTEL_EXPORTER_OTLP_METRICS_TEMPORALITY_PREFERENCE": "delta"
  }
}
```

- Hooks são `async` de propósito: com a placa desligada, um hook síncrono seguraria o Claude Code até o timeout em todo
  evento. `exit 0` evita que uma falha do `curl` vire código de bloqueio.
- Se ele já tiver uma `statusLine`, a página gera a variante que repassa a entrada para o comando antigo e mantém a
  saída dele.

### Endpoints

| Endpoint | Entrada | O que a placa extrai |
|---|---|---|
| `POST /api/claude/statusline` | JSON da barra de status | `session_id`, `model.display_name`, nome da pasta de `workspace.project_dir`, `context_window.used_percentage`, `cost.total_cost_usd`, `rate_limits.five_hour` e `rate_limits.seven_day` (`used_percentage`, `resets_at`) |
| `POST /api/claude/hook` | JSON do hook | `session_id`, `hook_event_name`, `notification_type`, `message`, `cwd` |
| `POST /v1/metrics` | OTLP/HTTP JSON | `claude_code.token.usage` (atributos `type`, `model`) e `claude_code.cost.usage`, somados no dia |

### Estado

- **Sessões** (até 8, a mais antiga sai): projeto, modelo, contexto %, atividade, último contato. Sem contato há
  30 min → removida.
- **Atividade**, despachada num único `switch` sobre o evento:

| Evento | Atividade | Na tela |
|---|---|---|
| `UserPromptSubmit` | trabalhando | chip "trabalhando…" |
| `Notification` + `permission_prompt` | esperando permissão | alerta forte (overlay) |
| `Notification` + `idle_prompt` | esperando você | chip "esperando você" |
| `Stop` | terminou | toast 5 s, depois ociosa |
| `SessionEnd` | — | sessão removida |

  Com várias sessões, a mais urgente manda: esperando permissão > esperando você > trabalhando > terminou > ociosa.
- **Limites**: últimos valores com o horário em que chegaram, persistidos no LittleFS. Passou o `resets_at`, mostra
  "zerou às HH:MM" e 0 %. Sem atualização há mais de 15 min, mostra "visto às HH:MM".
- **Totais diários**: por dia no fuso `America/Sao_Paulo` → tokens por tipo e por modelo + custo estimado. 30 dias em
  `/claude/daily.json`, gravado no máximo a cada 5 min para poupar a flash.

## 9. Configuração inicial

1. Liga → tela de aniversário (não precisa de rede). Toque → continua.
2. Sem Wi-Fi salvo → a placa abre a rede `Claudinho` com portal cativo; a tela mostra o QR dessa rede
   (`WIFI:S:Claudinho;T:nopass;;`). Ele escolhe a rede de casa no portal.
3. Conectou → a tela mostra o QR de `http://claudinho.local` (e o IP por extenso, caso o mDNS falhe).
4. Na página: define a senha de administrador, conecta o Spotify, copia o bloco do Claude Code, vê as fotos.
5. Configurado → a tela de configuração some e o Início assume.

A mensagem de aniversário (texto, data, assinatura) fica em `firmware/data/birthday.json`, editado por você antes de
gravar a placa.

## 10. Fotos

- Envio pela página web: o próprio navegador recorta e redimensiona para 480×320 e manda JPEG (~50 KB). Nada para
  instalar no PC.
- Guardadas em `/photos/` no LittleFS — cabem ~150. Você pré-carrega antes de embrulhar pela mesma página.
- Carrossel: troca a cada 12 s (configurável), com fade; ordem embaralhada, sem repetir até passar por todas.
- Listar e apagar fotos pela página.

## 11. Atualização de firmware

- Pela página web (upload do `.bin`) e por `pio run -t upload --upload-port claudinho.local`; os dois pedem a senha de
  administrador.
- Rollback: o firmware novo só se declara válido depois de 60 s rodando com a UI de pé. Reiniciou antes disso, o
  bootloader volta para o anterior.

## 12. Quando algo dá errado

| Situação | Comportamento |
|---|---|
| Sem Wi-Fi | fotos continuam; Spotify e Claude mostram "sem rede"; reconexão com recuo exponencial (até 60 s) |
| Hora não sincronizada | relógio escondido; "hoje" do Claude espera a hora chegar |
| Spotify desconectado | QR de reconectar na tela do Spotify |
| Nada tocando | "abra o Spotify em algum aparelho"; pílula do Início some |
| Dados do Claude parados | "visto às HH:MM" |
| JSON inválido em qualquer endpoint | `400` com o campo que falhou; estado anterior mantido |
| Flash cheia | upload de foto recusado com mensagem clara |
| Travamento | watchdog reinicia; core dump na partição `coredump` para depurar |

## 13. Segurança

- Página de configuração, fotos e OTA exigem a senha de administrador (HTTP Basic na rede local).
- Endpoints do Claude ficam abertos na rede local, porque o Claude Code não manda credencial. Pior caso: alguém na
  rede de casa forja um alerta. Risco aceito.
- O refresh token do Spotify não é exposto por nenhuma rota.
- O relay só redireciona para IP privado, para não virar redirecionador aberto.

## 14. Testes

- **No PC** (`pio test -e native`, Unity): a lógica não depende do Arduino — o hardware fica atrás de interfaces
  (`Display`, `Clock`, `HttpClient`). Testados: parsers (barra de status, hook, OTLP, player do Spotify), máquina de
  atividade das sessões, totais diários e virada do dia, PKCE (vetor da RFC 7636), intervalo de polling, embaralhamento
  das fotos, cor predominante.
- **Fixtures reais**: JSONs capturados do Claude Code de verdade (barra de status, cada hook, um export OTLP) em
  `test/fixtures/`.
- **Na placa**: roteiro manual em `docs/roteiro-de-teste.md` — touch, deslizar, cada tela, alerta, OTA, rollback,
  boot sem Wi-Fi.

## 15. Prioridades

Ordem de implementação, e de corte se faltar tempo (de baixo para cima):

| Prioridade | Itens |
|---|---|
| 0 — primeiro dia | teste mínimo na placa: tela + touch + LVGL em paisagem; integração do Claude Code testada no PC |
| Essencial | base (tarefas, estado publicado, navegação), Wi-Fi + portal, página de config com senha, fotos, Spotify (música atual + controles), limites e alertas do Claude, aniversário |
| Importante | tokens do dia (OTLP), cor da capa, OTA com rollback, modo noite |
| Capricho | pílula da música no Início, toast "terminou", volta automática ao Início, animação dos anéis |

## 16. Riscos — verificar no primeiro dia

1. LVGL 9 em full refresh com rotação por software: taxa de quadros aceitável? Plano B: retrato 320×480.
2. pioarduino + Arduino_GFX 1.6.0 + LVGL 9 compilam e rodam juntos.
3. Hooks `async` recebem o JSON pela entrada padrão e não seguram o Claude Code; a telemetria funciona com assinatura
   Pro/Max e respeita `delta`. Testável no seu PC (Windows também resolve `.local`).
4. O salto HTTPS → HTTP local do relay funciona no navegador do celular dele. Plano B: colar a URL.
5. A barra de status em segundo plano (`(… &)`) não é morta quando o comando termina.

## 17. Pendências (não bloqueiam o início)

- Nome definitivo.
- Conta do GitHub que hospeda o repositório e o relay (sua, ou transferida para ele depois).
- Texto da mensagem de aniversário.

## 18. Fora da v1 — viram issues para o pai

Clima, GitHub (contribuições, PRs, CI), cotações, recados e fotos por Telegram, placar de futebol, pomodoro, microSD,
som, simulador no PC, e uma API de "página genérica" para ele alimentar a tela de qualquer linguagem (Go incluso).
