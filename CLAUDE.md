# Claudinho — regras do projeto

Valem as regras de `~/.claude/CLAUDE.md`. Aqui só o que é específico deste firmware.

## Onde o código mora

- Pasta por conceito: `board/`, `navigation/`, `photos/`, `birthday/`, `setup/`, `support/` (e depois `spotify/`,
  `claude/`).
- Código preso a hardware ou framework fica numa subpasta folha do conceito: `device/` (GPIO, I2C, Wi-Fi, relógio),
  `screen/` (LVGL), `web/` (rotas HTTP), `storage/` (LittleFS, NVS). `theme/` é inteira LVGL.
- O resto é C++17 puro e roda no PC. O ambiente `native` exclui as pastas acima; se um arquivo de lógica pura precisa
  incluir Arduino ou LVGL, ele está no lugar errado.

## Regras de firmware

- Só o loop do Arduino (núcleo 1) chama LVGL. Outras tarefas publicam com `Published<T>` e a UI lê.
- Buffer grande (> 4 KB) vai para a PSRAM: `heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.
- Lógica pura usa `std::string`; `String` do Arduino só nas bordas.
- Validação não usa exceção: o value object tem `static std::optional<T> parse(...)` e construtor privado.
- **Nunca rode `pio run -t uploadfs`**: ele regrava a partição inteira e apaga as fotos. A página web vai embutida no
  firmware (`board_build.embed_files`).

## Testes

- `pio test -e native` roda toda a lógica pura no PC.
- `src/photos/ShuffleOrder.cpp` → `test/photos/test_shuffle_order/test_main.cpp`.
- JS da página: `node --test "test/web/*.test.mjs"` (o Node no Windows não aceita só a pasta).
- Relay do Spotify (da raiz): `node --test "relay/test/*.test.mjs"`.
- O que só se vê na placa entra em `docs/roteiro-de-teste.md`.

## Comandos (de dentro de `firmware/`)

- No Windows o core do PlatformIO fica em `C:\pio` (`PLATFORMIO_CORE_DIR`): na pasta padrão `~/.platformio` a
  extração do core do ESP32 estoura o limite de 260 caracteres de caminho.

- `pio run -e board -t upload` compila e grava; `pio device monitor` lê o serial.
- `pio test -e native` roda os testes.
