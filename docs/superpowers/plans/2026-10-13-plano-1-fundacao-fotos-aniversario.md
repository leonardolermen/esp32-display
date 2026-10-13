# Plano 1 — Fundação, fotos e aniversário — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a placa liga, mostra a mensagem de aniversário, se conecta ao Wi-Fi da casa pela própria tela, e vira um
porta-retrato com relógio cujas fotos chegam pelo navegador em `claudinho.local`, protegidas por senha.

**Architecture:** firmware PlatformIO/Arduino no ESP32-S3. A UI (LVGL 9, paisagem 480×320 girada por software para o
painel retrato do AXS15231B) roda só no loop do Arduino (núcleo 1); Wi-Fi, servidor web e relógio rodam numa tarefa
no núcleo 0 e entregam estado à UI por `Published<T>`. Lógica pura fica fora das pastas `device/`, `screen/`, `web/`,
`storage/` e é testada no PC com `pio test -e native`.

**Tech Stack:** PlatformIO (pioarduino 55.03.312 = Arduino core 3.3.12), LVGL 9.4.0, Arduino_GFX 1.6.9, ArduinoJson
7.4.2, JPEGDEC, ESPAsyncWebServer 3.x, WiFiManager 2.0.17, Unity, Node `node --test` para o JS da página.

**Spec:** `docs/superpowers/specs/2026-10-13-claudinho-design.md`

Este é o primeiro de quatro planos: **1 fundação, fotos e aniversário** → 2 Claude → 3 Spotify → 4 acabamento (OTA,
modo noite, extras). Os planos 2 e 3 são escritos depois da Tarefa 2 deste, porque o resultado do bring-up (taxa de
quadros, versões que funcionaram) muda detalhes deles.

## Desvios da especificação (decididos ao planejar)

- **Página web embutida no firmware** (`board_build.embed_files`), não no LittleFS. `pio run -t uploadfs` regrava a
  partição inteira e apagaria as fotos; embutida, a página também se atualiza junto com o firmware. Não existe pasta
  `data/` e **nunca** se roda `uploadfs`.
- **Mensagem de aniversário em `src/birthday/BirthdayContent.h`**, não em `birthday.json`: é texto fixo que você edita
  antes de gravar, e assim vale o mesmo motivo acima.
- **O servidor dá nome às fotos** (`p0001.jpg`, `p0002.jpg`…): nome vindo do cliente nunca vira caminho de arquivo.
- `Page` não tem `name()` (ninguém usa ainda).

## Global Constraints

- Placa: Guition JC3248W535C — ESP32-S3 N16R8 (16 MB flash QIO, 8 MB PSRAM OPI), painel 320×480 AXS15231B em QSPI,
  touch I2C endereço `0x3B`.
- Pinos: QSPI CS 45, SCK 47, D0 21, D1 48, D2 40, D3 39 · backlight 1 · touch SDA 4, SCL 8, reset 12.
- Tela lógica: **paisagem 480×320**; LVGL em `LV_DISPLAY_RENDER_MODE_FULL`; rotação por software.
- Cores: fundo `#1F1E1C`, superfície `#34322F`, texto `#F5F1EA`, secundário `#A8A29A`, laranja `#D97757`,
  âmbar `#E8B04A`.
- Fonte: Inter com Latin-1 (acentos do português).
- Fuso: `America/Sao_Paulo` → TZ POSIX `<-03>3`. NTP: `pool.ntp.org`, `time.google.com`.
- Hostname mDNS `claudinho` (`http://claudinho.local`); rede do portal cativo: `Claudinho`, aberta.
- Fotos: JPEG 480×320, até 200 KB cada, em `/photos/` no LittleFS (partição rotulada `spiffs`); reserva mínima de
  512 KB livres na flash.
- Senha de administrador: 8 a 64 caracteres; HTTP Basic com usuário `admin`.
- Código segue `~/.claude/CLAUDE.md`: nomes por extenso, chaves sempre, um comando por linha, até 120 colunas,
  continuação com 4 espaços, linha em branco entre blocos lógicos, comentário só de porquê, sem cadeia de `if`.
- Código preso a hardware ou framework só em subpastas folha `device/`, `screen/`, `web/`, `storage/`, e em `theme/`
  (inteira LVGL). O ambiente `native` exclui essas pastas.
- Teste espelha `src/`: `src/photos/ShuffleOrder.cpp` → `test/photos/test_shuffle_order/test_main.cpp`.
- Commits: conventional commits em português, terminando com `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- Finais de linha LF (`.gitattributes`), porque o repositório vai do Windows para o Mac.

## Review Focus

1. **Foto em retrato ou panorâmica** — o navegador recorta para 480×320 sem distorcer, puxando o recorte para o terço
   de cima nas fotos em pé (rostos). Teste: `test/web/crop.test.mjs` (Tarefa 7).
2. **Flash enchendo durante o envio** — upload recusado com mensagem clara e nenhum arquivo pela metade sobra. Teste:
   `test/photos/test_upload_check` (Tarefa 7) + limpeza de temporários no boot (roteiro, Tarefa 7).
3. **Sem Wi-Fi ou sem hora** — aniversário aparece no primeiro boot mesmo sem relógio; relógio fica escondido até o
   NTP. Teste: `test/birthday/test_birthday_calendar` (Tarefa 9) e `test/setup/test_setup_progress` (Tarefa 5).
4. **Nome de arquivo hostil** (`../x`, `a/b.jpg`, `x.png`) em apagar/baixar foto — recusado. Teste:
   `test/photos/test_photo_name` (Tarefa 7).
5. **Nenhuma foto ainda** — o Início mostra um convite para mandar fotos, não trava. Teste:
   `test/photos/test_shuffle_order` (caso vazio, Tarefa 8) + roteiro (Tarefa 8).

---

### Task 1: Ferramentas e lógica pura da placa (rotação e touch)

O AXS15231B não gira a imagem; a UI desenha em paisagem e cada quadro é girado para o painel retrato. O touch devolve
coordenadas do painel e passa pela rotação inversa. Esta tarefa instala as ferramentas e prova, no PC, que tela e
touch nunca discordam.

**Files:**
- Create: `.gitignore`, `.gitattributes`, `CLAUDE.md`
- Create: `firmware/platformio.ini`
- Create: `firmware/src/board/ScreenGeometry.h`
- Create: `firmware/src/board/ScreenRotation.h`, `firmware/src/board/ScreenRotation.cpp`
- Create: `firmware/src/board/AxsTouchReport.h`, `firmware/src/board/AxsTouchReport.cpp`
- Test: `firmware/test/board/test_screen_rotation/test_main.cpp`
- Test: `firmware/test/board/test_axs_touch_report/test_main.cpp`

**Interfaces:**
- Consumes: nada.
- Produces:
  - `constexpr uint16_t PANEL_WIDTH = 320, PANEL_HEIGHT = 480, SCREEN_WIDTH = 480, SCREEN_HEIGHT = 320;`
  - `struct PanelPoint { uint16_t x; uint16_t y; };` · `struct ScreenPoint { uint16_t x; uint16_t y; };`
  - `class ScreenRotation { virtual void rotate(const uint16_t* landscape, uint16_t* portrait) const = 0;
    virtual ScreenPoint toScreen(PanelPoint touched) const = 0; };` com `Rotation90` e `Rotation270`.
  - `class AxsTouchReport { static constexpr size_t LENGTH = 8;
    static std::optional<PanelPoint> parse(const uint8_t (&report)[LENGTH]); };`

- [ ] **Step 1: Instalar o PlatformIO e o compilador do PC**

Peça autorização ao usuário antes: instala programas no Windows.

```bash
uv tool install platformio
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e --accept-package-agreements --accept-source-agreements
```

Abra um terminal novo (o PATH muda) e confira:

```bash
pio --version
gcc --version
```

Expected: `PlatformIO Core, version 6.x` e `gcc.exe (MinGW-W64 ...) 14.x` ou mais novo. Se `pio` não for achado, rode
`uv tool update-shell` e abra outro terminal. No Mac do pai: `brew install platformio` e o `clang` do Xcode já bastam.

- [ ] **Step 2: Arquivos da raiz do repositório**

`.gitignore`:

```gitignore
firmware/.pio/
firmware/.vscode/
firmware/tools/fonts/.cache/
.DS_Store
```

`.gitattributes`:

```gitattributes
* text=auto eol=lf
*.png binary
*.jpg binary
*.ttf binary
```

`CLAUDE.md`:

```markdown
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
- JS da página: `node --test test/web/`.
- O que só se vê na placa entra em `docs/roteiro-de-teste.md`.

## Comandos (de dentro de `firmware/`)

- `pio run -e board -t upload` compila e grava; `pio device monitor` lê o serial.
- `pio test -e native` roda os testes.
```

- [ ] **Step 3: `platformio.ini` só com o ambiente de testes**

`firmware/platformio.ini`:

```ini
[platformio]
default_envs = native

[env:native]
platform = native
test_framework = unity
test_build_src = yes
build_flags =
    -std=gnu++17
    -I src
; Pastas presas a hardware ou framework não compilam no PC; veja CLAUDE.md.
build_src_filter =
    +<*>
    -<main.cpp>
    -<theme/>
    -<*/device/>
    -<*/screen/>
    -<*/web/>
    -<*/storage/>
lib_deps =
    bblanchon/ArduinoJson@7.4.2
```

- [ ] **Step 4: Escrever os testes que falham**

`firmware/test/board/test_screen_rotation/test_main.cpp`:

```cpp
#include <unity.h>

#include <vector>

#include "board/ScreenRotation.h"

namespace {

constexpr uint16_t MARK = 0xF800;

PanelPoint whereTheMarkLands(const ScreenRotation& rotation, ScreenPoint marked)
{
    std::vector<uint16_t> landscape(SCREEN_WIDTH * SCREEN_HEIGHT, 0);
    std::vector<uint16_t> portrait(PANEL_WIDTH * PANEL_HEIGHT, 0);
    landscape[marked.y * SCREEN_WIDTH + marked.x] = MARK;

    rotation.rotate(landscape.data(), portrait.data());

    for (size_t index = 0; index < portrait.size(); index++) {
        if (portrait[index] == MARK) {
            return PanelPoint{static_cast<uint16_t>(index % PANEL_WIDTH), static_cast<uint16_t>(index / PANEL_WIDTH)};
        }
    }
    TEST_FAIL_MESSAGE("the mark is missing from the rotated frame");
    return PanelPoint{0, 0};
}

void assertTouchLandsWhereDrawn(const ScreenRotation& rotation, ScreenPoint drawn)
{
    PanelPoint onPanel = whereTheMarkLands(rotation, drawn);

    ScreenPoint touched = rotation.toScreen(onPanel);

    TEST_ASSERT_EQUAL_UINT16(drawn.x, touched.x);
    TEST_ASSERT_EQUAL_UINT16(drawn.y, touched.y);
}

}

void setUp()
{
}

void tearDown()
{
}

void test_rotation90_puts_screen_origin_at_panel_top_right()
{
    PanelPoint onPanel = whereTheMarkLands(Rotation90(), ScreenPoint{0, 0});

    TEST_ASSERT_EQUAL_UINT16(PANEL_WIDTH - 1, onPanel.x);
    TEST_ASSERT_EQUAL_UINT16(0, onPanel.y);
}

void test_rotation270_puts_screen_origin_at_panel_bottom_left()
{
    PanelPoint onPanel = whereTheMarkLands(Rotation270(), ScreenPoint{0, 0});

    TEST_ASSERT_EQUAL_UINT16(0, onPanel.x);
    TEST_ASSERT_EQUAL_UINT16(PANEL_HEIGHT - 1, onPanel.y);
}

void test_rotation90_touch_lands_where_it_was_drawn()
{
    Rotation90 rotation;

    assertTouchLandsWhereDrawn(rotation, ScreenPoint{0, 0});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{123, 45});
}

void test_rotation270_touch_lands_where_it_was_drawn()
{
    Rotation270 rotation;

    assertTouchLandsWhereDrawn(rotation, ScreenPoint{0, 0});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{123, 45});
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_rotation90_puts_screen_origin_at_panel_top_right);
    RUN_TEST(test_rotation270_puts_screen_origin_at_panel_bottom_left);
    RUN_TEST(test_rotation90_touch_lands_where_it_was_drawn);
    RUN_TEST(test_rotation270_touch_lands_where_it_was_drawn);
    return UNITY_END();
}
```

`firmware/test/board/test_axs_touch_report/test_main.cpp`:

```cpp
#include <unity.h>

#include "board/AxsTouchReport.h"

void setUp()
{
}

void tearDown()
{
}

void test_reads_one_touch_point()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x01, 0x00, 0x64, 0x01, 0x2C, 0x00, 0x00};

    std::optional<PanelPoint> point = AxsTouchReport::parse(report);

    TEST_ASSERT_TRUE(point.has_value());
    TEST_ASSERT_EQUAL_UINT16(100, point->x);
    TEST_ASSERT_EQUAL_UINT16(300, point->y);
}

void test_ignores_event_bits_in_the_high_nibble()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x01, 0x80, 0x64, 0x41, 0x2C, 0x00, 0x00};

    std::optional<PanelPoint> point = AxsTouchReport::parse(report);

    TEST_ASSERT_TRUE(point.has_value());
    TEST_ASSERT_EQUAL_UINT16(100, point->x);
    TEST_ASSERT_EQUAL_UINT16(300, point->y);
}

void test_no_finger_means_no_point()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x00, 0x00, 0x64, 0x01, 0x2C, 0x00, 0x00};

    TEST_ASSERT_FALSE(AxsTouchReport::parse(report).has_value());
}

void test_more_points_than_requested_is_bus_noise()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x02, 0x00, 0x64, 0x01, 0x2C, 0x00, 0x00};

    TEST_ASSERT_FALSE(AxsTouchReport::parse(report).has_value());
}

void test_point_outside_the_panel_is_rejected()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x01, 0x01, 0x40, 0x01, 0x2C, 0x00, 0x00};

    TEST_ASSERT_FALSE(AxsTouchReport::parse(report).has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_reads_one_touch_point);
    RUN_TEST(test_ignores_event_bits_in_the_high_nibble);
    RUN_TEST(test_no_finger_means_no_point);
    RUN_TEST(test_more_points_than_requested_is_bus_noise);
    RUN_TEST(test_point_outside_the_panel_is_rejected);
    return UNITY_END();
}
```

- [ ] **Step 5: Rodar e ver falhar**

Run: `cd firmware && pio test -e native`
Expected: FAIL na compilação — `board/ScreenRotation.h: No such file or directory`.

- [ ] **Step 6: Implementar**

`firmware/src/board/ScreenGeometry.h`:

```cpp
#pragma once

#include <cstdint>

constexpr uint16_t PANEL_WIDTH = 320;
constexpr uint16_t PANEL_HEIGHT = 480;
constexpr uint16_t SCREEN_WIDTH = 480;
constexpr uint16_t SCREEN_HEIGHT = 320;

struct PanelPoint {
    uint16_t x;
    uint16_t y;
};

struct ScreenPoint {
    uint16_t x;
    uint16_t y;
};
```

`firmware/src/board/ScreenRotation.h`:

```cpp
#pragma once

#include "board/ScreenGeometry.h"

// O AXS15231B não gira a imagem por hardware: a UI desenha em paisagem e cada quadro é girado aqui para o painel
// retrato. O touch volta nas coordenadas do painel e passa pela mesma rotação ao contrário, então tela e toque
// nunca discordam. Rotation90 é a orientação do exemplo F1ATB (setRotation(1)); Rotation270 é a placa de cabeça
// para baixo.
class ScreenRotation {
public:
    virtual ~ScreenRotation() = default;

    virtual void rotate(const uint16_t* landscape, uint16_t* portrait) const = 0;
    virtual ScreenPoint toScreen(PanelPoint touched) const = 0;
};

class Rotation90 : public ScreenRotation {
public:
    void rotate(const uint16_t* landscape, uint16_t* portrait) const override;
    ScreenPoint toScreen(PanelPoint touched) const override;
};

class Rotation270 : public ScreenRotation {
public:
    void rotate(const uint16_t* landscape, uint16_t* portrait) const override;
    ScreenPoint toScreen(PanelPoint touched) const override;
};
```

`firmware/src/board/ScreenRotation.cpp`:

```cpp
#include "board/ScreenRotation.h"

void Rotation90::rotate(const uint16_t* landscape, uint16_t* portrait) const
{
    for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
        const uint16_t* row = landscape + y * SCREEN_WIDTH;
        uint16_t panelX = PANEL_WIDTH - 1 - y;

        for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
            portrait[x * PANEL_WIDTH + panelX] = row[x];
        }
    }
}

ScreenPoint Rotation90::toScreen(PanelPoint touched) const
{
    return ScreenPoint{touched.y, static_cast<uint16_t>(PANEL_WIDTH - 1 - touched.x)};
}

void Rotation270::rotate(const uint16_t* landscape, uint16_t* portrait) const
{
    for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
        const uint16_t* row = landscape + y * SCREEN_WIDTH;

        for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
            portrait[(PANEL_HEIGHT - 1 - x) * PANEL_WIDTH + y] = row[x];
        }
    }
}

ScreenPoint Rotation270::toScreen(PanelPoint touched) const
{
    return ScreenPoint{static_cast<uint16_t>(PANEL_HEIGHT - 1 - touched.y), touched.x};
}
```

`firmware/src/board/AxsTouchReport.h`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "board/ScreenGeometry.h"

// Resposta do AXS15231B a uma leitura de um ponto: [_, quantidade, xAlto, xBaixo, yAlto, yBaixo, _, _]. Só o nibble
// baixo dos bytes altos é coordenada; o nibble alto carrega o tipo de evento.
class AxsTouchReport {
public:
    static constexpr size_t LENGTH = 8;

    static std::optional<PanelPoint> parse(const uint8_t (&report)[LENGTH]);
};
```

`firmware/src/board/AxsTouchReport.cpp`:

```cpp
#include "board/AxsTouchReport.h"

std::optional<PanelPoint> AxsTouchReport::parse(const uint8_t (&report)[LENGTH])
{
    // A leitura pede um único ponto; qualquer quantidade diferente de 1 é "sem dedo" ou ruído do barramento.
    uint8_t touchCount = report[1];
    if (touchCount != 1) {
        return std::nullopt;
    }

    uint16_t x = static_cast<uint16_t>(((report[2] & 0x0F) << 8) | report[3]);
    uint16_t y = static_cast<uint16_t>(((report[4] & 0x0F) << 8) | report[5]);
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) {
        return std::nullopt;
    }

    return PanelPoint{x, y};
}
```

- [ ] **Step 7: Rodar e ver passar**

Run: `cd firmware && pio test -e native`
Expected: `9 test cases: 9 succeeded`.

- [ ] **Step 8: Commit**

```bash
git add .gitignore .gitattributes CLAUDE.md firmware/platformio.ini firmware/src/board firmware/test/board
git commit -m "chore(firmware): projeto PlatformIO com testes no PC; rotação da tela e leitura do touch

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: Bring-up — tela, touch e LVGL em paisagem

O maior risco técnico do projeto. Ao fim desta tarefa a placa mostra uma tela de teste em paisagem, responde ao toque
e exibe a taxa de quadros. Decide de vez as versões do driver e a orientação.

**Files:**
- Modify: `firmware/platformio.ini` (ambiente `board`)
- Create: `firmware/partitions.csv`
- Create: `firmware/include/lv_conf.h`
- Create: `firmware/src/board/BoardPins.h`
- Create: `firmware/src/board/device/Display.h`, `firmware/src/board/device/Display.cpp`
- Create: `firmware/src/board/device/Touch.h`, `firmware/src/board/device/Touch.cpp`
- Create: `firmware/src/board/device/Backlight.h`, `firmware/src/board/device/Backlight.cpp`
- Create: `firmware/src/main.cpp`
- Create: `docs/roteiro-de-teste.md`

**Interfaces:**
- Consumes: `ScreenRotation`, `Rotation90`, `AxsTouchReport`, constantes de `ScreenGeometry.h` (Tarefa 1).
- Produces:
  - `class Display { explicit Display(const ScreenRotation& rotation); lv_display_t* begin(); };`
  - `class Touch { explicit Touch(const ScreenRotation& rotation); lv_indev_t* begin(); };`
  - `class Backlight { void begin(); void setBrightness(uint8_t percent); };`
  - `namespace pins { LCD_CS, LCD_SCK, LCD_D0..LCD_D3, LCD_BACKLIGHT, TOUCH_SDA, TOUCH_SCL, TOUCH_RESET,
    TOUCH_ADDRESS }`

- [ ] **Step 1: Ambiente `board` no `platformio.ini`**

Troque `default_envs = native` por `default_envs = board` e acrescente:

```ini
[env:board]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.partitions = partitions.csv
board_build.filesystem = littlefs
monitor_speed = 115200
build_flags =
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D ARDUINO_USB_MODE=1
    -D BOARD_HAS_PSRAM
    -D LV_CONF_INCLUDE_SIMPLE
    -I include
lib_deps =
    lvgl/lvgl@9.4.0
    moononournation/GFX Library for Arduino@1.6.9
    bblanchon/ArduinoJson@7.4.2
```

- [ ] **Step 2: Tabela de partições**

`firmware/partitions.csv`:

```csv
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x5000
otadata,  data, ota,      0xe000,   0x2000
app0,     app,  ota_0,    0x10000,  0x300000
app1,     app,  ota_1,    0x310000, 0x300000
# O LittleFS do Arduino procura por padrão a partição rotulada "spiffs".
spiffs,   data, spiffs,   0x610000, 0x9E0000
coredump, data, coredump, 0xFF0000, 0x10000
```

- [ ] **Step 3: Configuração do LVGL**

`firmware/include/lv_conf.h`:

```c
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_DEF_REFR_PERIOD 16
#define LV_USE_OS LV_OS_NONE

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

#define LV_USE_SYSMON 1
#define LV_USE_PERF_MONITOR 1

#define LV_USE_QRCODE 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1

#endif
```

- [ ] **Step 4: Pinos**

`firmware/src/board/BoardPins.h`:

```cpp
#pragma once

#include <cstdint>

// Pinos do exemplo F1ATB para a JC3248W535C. O reset do touch (12) coincide com o SCLK do microSD nas fontes;
// enquanto o microSD estiver fora do projeto, isso não importa — confirmar no esquemático antes de usar o SD.
namespace pins {

constexpr int8_t LCD_CS = 45;
constexpr int8_t LCD_SCK = 47;
constexpr int8_t LCD_D0 = 21;
constexpr int8_t LCD_D1 = 48;
constexpr int8_t LCD_D2 = 40;
constexpr int8_t LCD_D3 = 39;
constexpr int8_t LCD_BACKLIGHT = 1;

constexpr int8_t TOUCH_SDA = 4;
constexpr int8_t TOUCH_SCL = 8;
constexpr int8_t TOUCH_RESET = 12;
constexpr uint8_t TOUCH_ADDRESS = 0x3B;

}
```

- [ ] **Step 5: Display**

`firmware/src/board/device/Display.h`:

```cpp
#pragma once

#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include <memory>

#include "board/ScreenRotation.h"

class Display {
public:
    explicit Display(const ScreenRotation& rotation);

    lv_display_t* begin();

private:
    static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);

    const ScreenRotation& rotation;
    std::unique_ptr<Arduino_ESP32QSPI> bus;
    std::unique_ptr<Arduino_AXS15231B> panel;
    uint16_t* screenFrame = nullptr;
    uint16_t* panelFrame = nullptr;
};
```

`firmware/src/board/device/Display.cpp`:

```cpp
#include "board/device/Display.h"

#include <esp_heap_caps.h>

#include "board/BoardPins.h"

namespace {

constexpr size_t FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);

uint16_t* allocateFrame()
{
    return static_cast<uint16_t*>(heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

}

Display::Display(const ScreenRotation& rotation)
    : rotation(rotation)
{
}

lv_display_t* Display::begin()
{
    bus = std::make_unique<Arduino_ESP32QSPI>(
        pins::LCD_CS, pins::LCD_SCK, pins::LCD_D0, pins::LCD_D1, pins::LCD_D2, pins::LCD_D3);
    // Desde a 1.6.1 o driver precisa da sequência de inicialização explícita; com a padrão a tela pisca
    // (issues #717 e #803 do Arduino_GFX). Se a imagem vier errada, a alternativa é a type2.
    panel = std::make_unique<Arduino_AXS15231B>(
        bus.get(), GFX_NOT_DEFINED, 0, false, PANEL_WIDTH, PANEL_HEIGHT, 0, 0, 0, 0,
        axs15231b_320480_type1_init_operations, sizeof(axs15231b_320480_type1_init_operations));
    if (!panel->begin()) {
        log_e("the display panel did not start");
    }
    panel->fillScreen(0x0000);

    screenFrame = allocateFrame();
    panelFrame = allocateFrame();

    lv_display_t* display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_user_data(display, this);
    lv_display_set_flush_cb(display, flush);
    // O AXS15231B corrompe a imagem com janelas parciais: o LVGL sempre renderiza e manda o quadro inteiro.
    lv_display_set_buffers(display, screenFrame, nullptr, FRAME_BYTES, LV_DISPLAY_RENDER_MODE_FULL);

    return display;
}

void Display::flush(lv_display_t* display, const lv_area_t*, uint8_t* pixels)
{
    Display* self = static_cast<Display*>(lv_display_get_user_data(display));

    self->rotation.rotate(reinterpret_cast<const uint16_t*>(pixels), self->panelFrame);
    self->panel->draw16bitRGBBitmap(0, 0, self->panelFrame, PANEL_WIDTH, PANEL_HEIGHT);

    lv_display_flush_ready(display);
}
```

- [ ] **Step 6: Touch**

`firmware/src/board/device/Touch.h`:

```cpp
#pragma once

#include <lvgl.h>

#include <optional>

#include "board/ScreenRotation.h"

class Touch {
public:
    explicit Touch(const ScreenRotation& rotation);

    lv_indev_t* begin();

private:
    static void read(lv_indev_t* device, lv_indev_data_t* data);
    std::optional<PanelPoint> readPanel();

    const ScreenRotation& rotation;
};
```

`firmware/src/board/device/Touch.cpp`:

```cpp
#include "board/device/Touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "board/AxsTouchReport.h"
#include "board/BoardPins.h"

namespace {

constexpr uint32_t I2C_CLOCK_HZ = 400000;
constexpr uint32_t RESET_PULSE_MILLIS = 200;

// Pedido de leitura do AXS15231B: cabeçalho B5 AB A5 5A e o tamanho da resposta (1 ponto × 6 bytes + 2).
constexpr uint8_t READ_ONE_POINT[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, AxsTouchReport::LENGTH, 0x00, 0x00, 0x00};

}

Touch::Touch(const ScreenRotation& rotation)
    : rotation(rotation)
{
}

lv_indev_t* Touch::begin()
{
    pinMode(pins::TOUCH_RESET, OUTPUT);
    digitalWrite(pins::TOUCH_RESET, LOW);
    delay(RESET_PULSE_MILLIS);
    digitalWrite(pins::TOUCH_RESET, HIGH);
    delay(RESET_PULSE_MILLIS);

    Wire.begin(pins::TOUCH_SDA, pins::TOUCH_SCL);
    Wire.setClock(I2C_CLOCK_HZ);

    lv_indev_t* device = lv_indev_create();
    lv_indev_set_type(device, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(device, read);
    lv_indev_set_user_data(device, this);

    return device;
}

std::optional<PanelPoint> Touch::readPanel()
{
    Wire.beginTransmission(pins::TOUCH_ADDRESS);
    Wire.write(READ_ONE_POINT, sizeof(READ_ONE_POINT));
    if (Wire.endTransmission() != 0) {
        return std::nullopt;
    }

    size_t received = Wire.requestFrom(
        static_cast<uint16_t>(pins::TOUCH_ADDRESS), static_cast<size_t>(AxsTouchReport::LENGTH), true);
    if (received != AxsTouchReport::LENGTH) {
        return std::nullopt;
    }

    uint8_t report[AxsTouchReport::LENGTH] = {};
    Wire.readBytes(report, AxsTouchReport::LENGTH);

    return AxsTouchReport::parse(report);
}

void Touch::read(lv_indev_t* device, lv_indev_data_t* data)
{
    Touch* self = static_cast<Touch*>(lv_indev_get_user_data(device));

    std::optional<PanelPoint> touched = self->readPanel();
    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    ScreenPoint point = self->rotation.toScreen(*touched);
    data->point.x = point.x;
    data->point.y = point.y;
    data->state = LV_INDEV_STATE_PRESSED;
}
```

- [ ] **Step 7: Backlight**

`firmware/src/board/device/Backlight.h`:

```cpp
#pragma once

#include <cstdint>

class Backlight {
public:
    void begin();
    void setBrightness(uint8_t percent);
};
```

`firmware/src/board/device/Backlight.cpp`:

```cpp
#include "board/device/Backlight.h"

#include <Arduino.h>

#include <algorithm>

#include "board/BoardPins.h"

namespace {

constexpr uint32_t PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint32_t PWM_MAX_DUTY = 255;

}

void Backlight::begin()
{
    ledcAttach(pins::LCD_BACKLIGHT, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
    setBrightness(100);
}

void Backlight::setBrightness(uint8_t percent)
{
    uint32_t clamped = std::min<uint32_t>(percent, 100);

    ledcWrite(pins::LCD_BACKLIGHT, clamped * PWM_MAX_DUTY / 100);
}
```

- [ ] **Step 8: Tela de bring-up**

`firmware/src/main.cpp`:

```cpp
#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>

#include "board/ScreenRotation.h"
#include "board/device/Backlight.h"
#include "board/device/Display.h"
#include "board/device/Touch.h"

namespace {

constexpr uint32_t MAX_LOOP_SLEEP_MILLIS = 10;

Rotation90 rotation;
Display display(rotation);
Touch touch(rotation);
Backlight backlight;
uint32_t taps = 0;

void logToSerial(lv_log_level_t, const char* text)
{
    Serial.print(text);
}

lv_obj_t* addCornerMarker(lv_obj_t* screen, lv_align_t corner, uint32_t color)
{
    lv_obj_t* marker = lv_obj_create(screen);
    lv_obj_set_size(marker, 40, 40);
    lv_obj_set_style_bg_color(marker, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(marker, 0, 0);
    lv_obj_set_style_radius(marker, 0, 0);
    lv_obj_align(marker, corner, 0, 0);
    return marker;
}

void countTap(lv_event_t* event)
{
    lv_obj_t* label = static_cast<lv_obj_t*>(lv_event_get_user_data(event));

    taps++;
    lv_label_set_text_fmt(label, "Toques: %lu", static_cast<unsigned long>(taps));
}

void printTouchedPoint(lv_event_t*)
{
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);

    Serial.printf("toque em x=%d y=%d\n", static_cast<int>(point.x), static_cast<int>(point.y));
}

void showBringUpScreen()
{
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1F1E1C), 0);
    lv_obj_add_event_cb(screen, printTouchedPoint, LV_EVENT_PRESSED, nullptr);

    addCornerMarker(screen, LV_ALIGN_TOP_LEFT, 0xFF0000);
    addCornerMarker(screen, LV_ALIGN_TOP_RIGHT, 0x00FF00);
    addCornerMarker(screen, LV_ALIGN_BOTTOM_LEFT, 0x0000FF);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Claudinho - bring-up");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF5F1EA), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* button = lv_button_create(screen);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t* buttonLabel = lv_label_create(button);
    lv_label_set_text(buttonLabel, "Toques: 0");
    lv_obj_add_event_cb(button, countTap, LV_EVENT_CLICKED, buttonLabel);

    lv_obj_t* spinner = lv_spinner_create(screen);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 60);
}

}

void setup()
{
    Serial.begin(115200);

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });
    lv_log_register_print_cb(logToSerial);

    display.begin();
    touch.begin();
    backlight.begin();

    showBringUpScreen();
}

void loop()
{
    uint32_t untilNextRun = lv_timer_handler();

    delay(std::min(untilNextRun, MAX_LOOP_SLEEP_MILLIS));
}
```

- [ ] **Step 9: Compilar**

Run: `cd firmware && pio run -e board`
Expected: `SUCCESS`. A primeira vez baixa a plataforma (~1 GB, demora). Se der
`'axs15231b_320480_type1_init_operations' was not declared`, a versão do Arduino_GFX resolvida não é a 1.6.9: confira
com `pio pkg list -e board`.

- [ ] **Step 10: Gravar e olhar a placa**

Run: `cd firmware && pio run -e board -t upload --upload-port COM8 && pio device monitor --port COM8`

Confira, nesta ordem:

| O que olhar | Esperado | Se não |
|---|---|---|
| Imagem estável, sem piscar ou listras | estável | troque `type1` por `type2` no `Display.cpp`; persistindo, veja o plano B abaixo |
| Texto legível, não espelhado | "Claudinho - bring-up" no alto | confira que `Rotation90` está em uso |
| Cantos | vermelho no alto à esquerda, verde no alto à direita, azul embaixo à esquerda | se tudo estiver de cabeça para baixo em relação a como a placa vai ficar na mesa, troque `Rotation90 rotation;` por `Rotation270 rotation;` em `main.cpp` |
| Cores | vermelho é vermelho, azul é azul | se vierem trocados, troque `draw16bitRGBBitmap` por `draw16bitBeRGBBitmap` no `Display::flush` e rode de novo |
| Toque no botão | contador sobe | veja o serial: as coordenadas de cada toque devem bater com o lugar tocado (canto vermelho ≈ x 20, y 20) |
| Taxa de quadros | o monitor do LVGL (canto inferior direito) mostra ≥ 20 FPS com o spinner girando | anote o número; < 15 FPS aciona o plano B de retrato da especificação, a decidir com o usuário |

**Plano B do driver** (só se `type1` e `type2` falharem): troque as linhas de `platform` e do Arduino_GFX por uma
release do pioarduino com Arduino core ≤ 3.3.5 (veja a lista em
https://github.com/pioarduino/platform-espressif32/releases e use a URL
`.../releases/download/<tag>/platform-espressif32.zip`) e `moononournation/GFX Library for Arduino@1.6.0`; nesse caso
o construtor do `Arduino_AXS15231B` volta a ser `(bus.get(), GFX_NOT_DEFINED, 0, false, PANEL_WIDTH, PANEL_HEIGHT)`.

- [ ] **Step 11: Registrar no roteiro de teste**

`docs/roteiro-de-teste.md`:

```markdown
# Roteiro de teste na placa

O que os testes do PC não alcançam. Rode a seção da área que você mexeu antes de gravar a versão que vai para a mesa.

## Tela e touch

- [ ] Imagem estável, sem piscar.
- [ ] Texto legível em paisagem, do jeito que a placa fica na mesa.
- [ ] Toque cai onde o dedo encostou (cantos e centro).
- [ ] Taxa de quadros com animação: ≥ 20 FPS.

Resultado do bring-up (preencher): versões usadas ______, init `type1`/`type2` ______, rotação ______, FPS ______.
```

Preencha a última linha com o que foi observado.

- [ ] **Step 12: Commit**

```bash
git add firmware/platformio.ini firmware/partitions.csv firmware/include firmware/src docs/roteiro-de-teste.md
git commit -m "feat(board): tela e touch em paisagem com LVGL 9 em full refresh

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: Estado publicado, registro de páginas e navegação por deslize

**Files:**
- Create: `firmware/src/support/Published.h`
- Create: `firmware/src/navigation/Page.h`
- Create: `firmware/src/navigation/PageIndex.h`, `firmware/src/navigation/PageIndex.cpp`
- Create: `firmware/src/navigation/screen/Navigator.h`, `firmware/src/navigation/screen/Navigator.cpp`
- Test: `firmware/test/navigation/test_page_index/test_main.cpp`
- Test: `firmware/test/support/test_published/test_main.cpp`

**Interfaces:**
- Consumes: LVGL (só em `screen/`), `SCREEN_WIDTH` de `board/ScreenGeometry.h`.
- Produces:
  - `template <typename T> class Published { void publish(const T&); bool readIfChanged(T& out, uint32_t& seenVersion) const; T read() const; };`
    No PC o mutex é `std::mutex`; na placa, `SemaphoreHandle_t` (via `#ifdef ARDUINO`).
  - `class Page { virtual void build(lv_obj_t* parent) = 0; virtual void onShow() {} virtual void onHide() {} virtual void onTick(uint32_t nowMillis) {} };`
  - `class PageIndex { explicit PageIndex(size_t count); size_t current() const; bool next(); bool previous(); void jumpTo(size_t); };`
  - `class Navigator { void add(std::unique_ptr<Page>); void begin(); void tick(uint32_t nowMillis); void goHome(); }`
    — cada página vive num `lv_obj_t` filho da tela; deslizar troca com animação de 250 ms; pontinhos embaixo
    (6 px, ativo 12 px).

- [ ] **Step 1: Testes de `PageIndex` e `Published`**

`firmware/test/navigation/test_page_index/test_main.cpp`:

```cpp
#include <unity.h>

#include "navigation/PageIndex.h"

void setUp()
{
}

void tearDown()
{
}

void test_starts_at_home()
{
    PageIndex index(3);

    TEST_ASSERT_EQUAL_size_t(0, index.current());
}

void test_next_stops_at_the_last_page()
{
    PageIndex index(3);

    TEST_ASSERT_TRUE(index.next());
    TEST_ASSERT_TRUE(index.next());
    TEST_ASSERT_FALSE(index.next());
    TEST_ASSERT_EQUAL_size_t(2, index.current());
}

void test_previous_stops_at_home()
{
    PageIndex index(3);

    TEST_ASSERT_FALSE(index.previous());
    TEST_ASSERT_EQUAL_size_t(0, index.current());
}

void test_jump_to_clamps_to_the_last_page()
{
    PageIndex index(3);

    index.jumpTo(9);

    TEST_ASSERT_EQUAL_size_t(2, index.current());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_at_home);
    RUN_TEST(test_next_stops_at_the_last_page);
    RUN_TEST(test_previous_stops_at_home);
    RUN_TEST(test_jump_to_clamps_to_the_last_page);
    return UNITY_END();
}
```

`firmware/test/support/test_published/test_main.cpp`:

```cpp
#include <unity.h>

#include "support/Published.h"

struct Weather {
    int degrees;
};

void setUp()
{
}

void tearDown()
{
}

void test_reader_sees_a_new_value_once()
{
    Published<Weather> published;
    uint32_t seen = 0;
    Weather weather{};

    published.publish(Weather{25});

    TEST_ASSERT_TRUE(published.readIfChanged(weather, seen));
    TEST_ASSERT_EQUAL_INT(25, weather.degrees);
    TEST_ASSERT_FALSE(published.readIfChanged(weather, seen));
}

void test_nothing_published_means_nothing_changed()
{
    Published<Weather> published;
    uint32_t seen = 0;
    Weather weather{};

    TEST_ASSERT_FALSE(published.readIfChanged(weather, seen));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_reader_sees_a_new_value_once);
    RUN_TEST(test_nothing_published_means_nothing_changed);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native`
Expected: FAIL — `navigation/PageIndex.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`firmware/src/support/Published.h`:

```cpp
#pragma once

#include <cstdint>

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <mutex>
#endif

// Ponte entre a tarefa de rede e a UI: a rede publica uma cópia, a UI lê quando desenha. Nenhum lado segura o outro
// por mais do que uma cópia de struct, e a UI só redesenha quando a versão mudou.
template <typename T>
class Published {
public:
    void publish(const T& value)
    {
        Lock lock(mutex);

        current = value;
        version++;
    }

    bool readIfChanged(T& out, uint32_t& seenVersion) const
    {
        Lock lock(mutex);

        if (version == seenVersion) {
            return false;
        }

        out = current;
        seenVersion = version;
        return true;
    }

    T read() const
    {
        Lock lock(mutex);

        return current;
    }

private:
#ifdef ARDUINO
    struct Mutex {
        SemaphoreHandle_t handle = xSemaphoreCreateMutex();
    };

    struct Lock {
        explicit Lock(const Mutex& mutex)
            : handle(mutex.handle)
        {
            xSemaphoreTake(handle, portMAX_DELAY);
        }

        ~Lock()
        {
            xSemaphoreGive(handle);
        }

        SemaphoreHandle_t handle;
    };
#else
    using Mutex = std::mutex;
    using Lock = std::lock_guard<std::mutex>;
#endif

    mutable Mutex mutex;
    T current{};
    uint32_t version = 0;
};
```

`firmware/src/navigation/Page.h`:

```cpp
#pragma once

#include <lvgl.h>

#include <cstdint>

// Uma página é um módulo: build cria os widgets uma vez, onTick lê o estado publicado e muda só o que mudou.
class Page {
public:
    virtual ~Page() = default;

    virtual void build(lv_obj_t* parent) = 0;

    virtual void onShow()
    {
    }

    virtual void onHide()
    {
    }

    virtual void onTick(uint32_t nowMillis)
    {
    }
};
```

`firmware/src/navigation/PageIndex.h`:

```cpp
#pragma once

#include <cstddef>

class PageIndex {
public:
    explicit PageIndex(size_t count);

    size_t current() const;
    bool next();
    bool previous();
    void jumpTo(size_t target);

private:
    size_t count;
    size_t index = 0;
};
```

`firmware/src/navigation/PageIndex.cpp`:

```cpp
#include "navigation/PageIndex.h"

#include <algorithm>

PageIndex::PageIndex(size_t count)
    : count(count)
{
}

size_t PageIndex::current() const
{
    return index;
}

bool PageIndex::next()
{
    if (index + 1 >= count) {
        return false;
    }

    index++;
    return true;
}

bool PageIndex::previous()
{
    if (index == 0) {
        return false;
    }

    index--;
    return true;
}

void PageIndex::jumpTo(size_t target)
{
    index = std::min(target, count - 1);
}
```

`firmware/src/navigation/screen/Navigator.h`:

```cpp
#pragma once

#include <lvgl.h>

#include <memory>
#include <vector>

#include "navigation/Page.h"
#include "navigation/PageIndex.h"

class Navigator {
public:
    void add(std::unique_ptr<Page> page);
    void begin();
    void tick(uint32_t nowMillis);
    void goHome();

private:
    struct Slot {
        std::unique_ptr<Page> page;
        lv_obj_t* container = nullptr;
        lv_obj_t* dot = nullptr;
    };

    static void onGesture(lv_event_t* event);
    void show(size_t target, lv_dir_t direction);
    void refreshDots();

    std::vector<Slot> slots;
    std::unique_ptr<PageIndex> index;
    lv_obj_t* dots = nullptr;
};
```

`firmware/src/navigation/screen/Navigator.cpp`:

```cpp
#include "navigation/screen/Navigator.h"

#include "board/ScreenGeometry.h"

namespace {

constexpr uint32_t SLIDE_MILLIS = 250;
constexpr int32_t DOT_SIZE = 6;
constexpr int32_t ACTIVE_DOT_WIDTH = 12;

void moveX(void* object, int32_t x)
{
    lv_obj_set_x(static_cast<lv_obj_t*>(object), x);
}

}

void Navigator::add(std::unique_ptr<Page> page)
{
    slots.push_back(Slot{std::move(page)});
}

void Navigator::begin()
{
    index = std::make_unique<PageIndex>(slots.size());

    lv_obj_t* screen = lv_screen_active();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, onGesture, LV_EVENT_GESTURE, this);

    for (Slot& slot : slots) {
        slot.container = lv_obj_create(screen);
        lv_obj_remove_style_all(slot.container);
        lv_obj_set_size(slot.container, LV_PCT(100), LV_PCT(100));
        lv_obj_add_flag(slot.container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(slot.container, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_clear_flag(slot.container, LV_OBJ_FLAG_SCROLLABLE);
        slot.page->build(slot.container);
    }

    dots = lv_obj_create(screen);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, DOT_SIZE);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, 5, 0);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -7);
    for (Slot& slot : slots) {
        slot.dot = lv_obj_create(dots);
        lv_obj_remove_style_all(slot.dot);
        lv_obj_set_style_radius(slot.dot, 3, 0);
        lv_obj_set_style_bg_color(slot.dot, lv_color_white(), 0);
    }

    lv_obj_clear_flag(slots[0].container, LV_OBJ_FLAG_HIDDEN);
    slots[0].page->onShow();
    refreshDots();
}

void Navigator::tick(uint32_t nowMillis)
{
    slots[index->current()].page->onTick(nowMillis);
}

void Navigator::goHome()
{
    if (index->current() != 0) {
        show(0, LV_DIR_RIGHT);
    }
}

void Navigator::onGesture(lv_event_t* event)
{
    Navigator* self = static_cast<Navigator*>(lv_event_get_user_data(event));
    lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_active());
    size_t before = self->index->current();

    if (direction == LV_DIR_LEFT && before + 1 < self->slots.size()) {
        self->show(before + 1, direction);
    }
    if (direction == LV_DIR_RIGHT && before > 0) {
        self->show(before - 1, direction);
    }
}

void Navigator::show(size_t target, lv_dir_t direction)
{
    Slot& leaving = slots[index->current()];
    Slot& entering = slots[target];

    leaving.page->onHide();
    lv_obj_add_flag(leaving.container, LV_OBJ_FLAG_HIDDEN);

    int32_t from = direction == LV_DIR_LEFT ? SCREEN_WIDTH : -SCREEN_WIDTH;
    lv_obj_set_x(entering.container, from);
    lv_obj_clear_flag(entering.container, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t slide;
    lv_anim_init(&slide);
    lv_anim_set_var(&slide, entering.container);
    lv_anim_set_values(&slide, from, 0);
    lv_anim_set_duration(&slide, SLIDE_MILLIS);
    lv_anim_set_path_cb(&slide, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&slide, moveX);
    lv_anim_start(&slide);

    index->jumpTo(target);
    entering.page->onShow();
    refreshDots();
    lv_obj_move_foreground(dots);
}

void Navigator::refreshDots()
{
    for (size_t position = 0; position < slots.size(); position++) {
        bool active = position == index->current();

        lv_obj_set_size(slots[position].dot, active ? ACTIVE_DOT_WIDTH : DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_bg_opa(slots[position].dot, active ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native`
Expected: `15 test cases: 15 succeeded`.

- [ ] **Step 5: Ligar no `main.cpp`**

Substitua `showBringUpScreen()` por duas páginas de exemplo (classes locais em `main.cpp`, cada uma com um label
"Página A" / "Página B" em cores diferentes), registradas em um `Navigator navigator;` global:

```cpp
navigator.add(std::make_unique<ExamplePage>("Página A", 0x3B6D11));
navigator.add(std::make_unique<ExamplePage>("Página B", 0x185FA5));
navigator.begin();
```

e no `loop()`, antes do `lv_timer_handler()`: `navigator.tick(millis());`.

Grave e confira: deslizar para a esquerda avança com animação, para a direita volta, nas pontas nada acontece, os
pontinhos acompanham. Acrescente ao roteiro:

```markdown
## Navegação

- [ ] Deslizar para a esquerda avança; para a direita volta; nas pontas não acontece nada.
- [ ] Pontinhos acompanham a página atual.
```

- [ ] **Step 6: Commit**

```bash
git add firmware/src firmware/test docs/roteiro-de-teste.md
git commit -m "feat(navigation): páginas como módulos, deslize entre elas e estado publicado

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: Tema e fontes Inter

**Files:**
- Create: `firmware/tools/fonts/build-fonts.sh`
- Create: `firmware/src/theme/Fonts.h`, `firmware/src/theme/Theme.h`, `firmware/src/theme/Theme.cpp`
- Create (gerados): `firmware/src/theme/inter_12.c`, `inter_14.c`, `inter_18.c`, `inter_24.c`, `inter_64_digits.c`
- Modify: `firmware/include/lv_conf.h` (remover `LV_FONT_MONTSERRAT_20`; manter a 14 para o monitor de FPS)

**Interfaces:**
- Produces:
  - `namespace fonts { small (12); medium (14); large (18, Medium); title (24, Medium); clock_ (64, só dígitos e ':'); }`
  - `namespace theme { lv_color_t background(); surface(); text(); textSecondary(); accent(); amber();
    void apply(lv_display_t*); lv_obj_t* label(lv_obj_t* parent, const lv_font_t*, lv_color_t); lv_obj_t* panel(lv_obj_t* parent); }`

- [ ] **Step 1: Script de geração**

`firmware/tools/fonts/build-fonts.sh`:

```bash
#!/usr/bin/env bash
# Gera as fontes Inter para o LVGL. Rode de dentro de firmware/tools/fonts/ depois do primeiro `pio run -e board`
# (o FontAwesome vem de dentro do pacote do LVGL). Precisa de node (npx). Os .c gerados são commitados.
set -euo pipefail

CACHE=.cache
OUT=../../src/theme
FA=../../.pio/libdeps/board/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff

mkdir -p "$CACHE"
if [ ! -f "$CACHE/Inter-Regular.ttf" ]; then
    curl -sL -o "$CACHE/inter.zip" https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
    unzip -o -j "$CACHE/inter.zip" 'extras/ttf/Inter-Regular.ttf' 'extras/ttf/Inter-Medium.ttf' -d "$CACHE"
fi

# Latin-1 completo cobre os acentos do português. Os símbolos LVGL (LV_SYMBOL_*) entram só nas fontes de texto.
LATIN='0x20-0x7E,0xA0-0xFF'
SYMBOLS='0x2022,0xF001,0xF008,0xF00B,0xF00C,0xF00D,0xF011,0xF013,0xF015,0xF019,0xF01C,0xF021,0xF026,0xF027,0xF028,0xF03E,0xF043,0xF048,0xF04B,0xF04C,0xF04D,0xF051,0xF052,0xF053,0xF054,0xF067,0xF068,0xF06E,0xF070,0xF071,0xF074,0xF077,0xF078,0xF079,0xF07B,0xF093,0xF095,0xF0C4,0xF0C5,0xF0C7,0xF0C9,0xF0E0,0xF0E7,0xF0EA,0xF0F3,0xF11C,0xF124,0xF158,0xF1EB,0xF240,0xF241,0xF242,0xF243,0xF244,0xF287,0xF293,0xF2ED,0xF304,0xF55A,0xF7C2,0xF8A2'

text_font() {
    local size=$1 weight=$2 name=$3
    npx --yes lv_font_conv@1.5.3 --bpp 4 --size "$size" --format lvgl --no-compress --lv-include lvgl.h \
        --font "$CACHE/Inter-$weight.ttf" --range "$LATIN" \
        --font "$FA" --range "$SYMBOLS" \
        -o "$OUT/$name.c"
}

text_font 12 Regular inter_12
text_font 14 Regular inter_14
text_font 18 Medium inter_18
text_font 24 Medium inter_24

npx --yes lv_font_conv@1.5.3 --bpp 4 --size 64 --format lvgl --no-compress --lv-include lvgl.h \
    --font "$CACHE/Inter-Medium.ttf" --range '0x30-0x3A' -o "$OUT/inter_64_digits.c"
```

Run: `cd firmware/tools/fonts && chmod +x build-fonts.sh && ./build-fonts.sh`
Expected: cinco arquivos `.c` em `firmware/src/theme/`. Se o `.woff` tiver outro nome, ajuste `FA` (`ls` na pasta).

- [ ] **Step 2: `Fonts.h`, `Theme.h`, `Theme.cpp`**

`firmware/src/theme/Fonts.h`:

```cpp
#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(inter_12)
LV_FONT_DECLARE(inter_14)
LV_FONT_DECLARE(inter_18)
LV_FONT_DECLARE(inter_24)
LV_FONT_DECLARE(inter_64_digits)

namespace fonts {

inline const lv_font_t* small = &inter_12;
inline const lv_font_t* medium = &inter_14;
inline const lv_font_t* large = &inter_18;
inline const lv_font_t* title = &inter_24;
inline const lv_font_t* clock_ = &inter_64_digits;

}
```

`firmware/src/theme/Theme.h`:

```cpp
#pragma once

#include <lvgl.h>

namespace theme {

lv_color_t background();
lv_color_t surface();
lv_color_t text();
lv_color_t textSecondary();
lv_color_t accent();
lv_color_t amber();

void apply(lv_display_t* display);
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color);
lv_obj_t* panel(lv_obj_t* parent);

}
```

`firmware/src/theme/Theme.cpp`:

```cpp
#include "theme/Theme.h"

#include "theme/Fonts.h"

namespace theme {

lv_color_t background()
{
    return lv_color_hex(0x1F1E1C);
}

lv_color_t surface()
{
    return lv_color_hex(0x34322F);
}

lv_color_t text()
{
    return lv_color_hex(0xF5F1EA);
}

lv_color_t textSecondary()
{
    return lv_color_hex(0xA8A29A);
}

lv_color_t accent()
{
    return lv_color_hex(0xD97757);
}

lv_color_t amber()
{
    return lv_color_hex(0xE8B04A);
}

void apply(lv_display_t* display)
{
    lv_theme_t* base = lv_theme_default_init(display, accent(), amber(), true, fonts::medium);
    lv_display_set_theme(display, base);

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, background(), 0);
    lv_obj_set_style_text_color(screen, text(), 0);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color)
{
    lv_obj_t* created = lv_label_create(parent);
    lv_obj_set_style_text_font(created, font, 0);
    lv_obj_set_style_text_color(created, color, 0);
    return created;
}

lv_obj_t* panel(lv_obj_t* parent)
{
    lv_obj_t* created = lv_obj_create(parent);
    lv_obj_remove_style_all(created);
    lv_obj_set_style_bg_color(created, surface(), 0);
    lv_obj_set_style_bg_opa(created, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(created, 10, 0);
    lv_obj_set_style_pad_all(created, 12, 0);
    return created;
}

}
```

- [ ] **Step 3: Usar no `main.cpp`**

Depois de `display.begin()` guarde o `lv_display_t*` e chame `theme::apply(lvDisplay);`. Nas páginas de exemplo use
`theme::label(parent, fonts::title, theme::text())` com um texto acentuado: `"Página — ação, coração"`.

- [ ] **Step 4: Compilar, gravar, conferir acentos**

Run: `cd firmware && pio run -e board -t upload --upload-port COM8`
Expected: os acentos aparecem nítidos. Roteiro: `- [ ] Acentos (ç, ã, é) aparecem nas fontes.`

- [ ] **Step 5: Commits (dois)**

```bash
git add firmware/tools/fonts/build-fonts.sh firmware/src/theme/inter_*.c
git commit -m "chore(theme): fontes Inter geradas para o LVGL

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git add firmware/src firmware/include docs/roteiro-de-teste.md
git commit -m "feat(theme): tema escuro com o laranja do Claude

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: Wi-Fi, portal cativo, mDNS, relógio e tela de configuração

**Files:**
- Create: `firmware/src/setup/SetupProgress.h`, `firmware/src/setup/SetupProgress.cpp`
- Create: `firmware/src/setup/device/WifiLink.h`, `firmware/src/setup/device/WifiLink.cpp`
- Create: `firmware/src/support/LocalTime.h`, `firmware/src/support/LocalTime.cpp`
- Create: `firmware/src/support/device/NetworkClock.h`, `firmware/src/support/device/NetworkClock.cpp`
- Create: `firmware/src/setup/screen/SetupPage.h`, `firmware/src/setup/screen/SetupPage.cpp`
- Test: `firmware/test/setup/test_setup_progress/test_main.cpp`
- Test: `firmware/test/support/test_local_time/test_main.cpp`
- Modify: `firmware/platformio.ini` (`lib_deps` do `board`: `tzapu/WiFiManager@2.0.17`)

**Interfaces:**
- Consumes: `Page`, `Published<T>`, `theme`, `fonts`.
- Produces:
  - `enum class SetupStep { WaitingWifi, WaitingAdmin, Done };`
  - `struct SetupState { SetupStep step = SetupStep::WaitingWifi; std::string qrText; std::string ip; };`
  - `class SetupProgress { static SetupState compute(bool wifiConnected, const std::string& ip, bool adminPasswordSet); };`
    — `qrText` é `WIFI:S:Claudinho;T:nopass;;` sem Wi-Fi e `http://claudinho.local` depois.
  - `class WifiLink { void begin(); void process(); bool connected() const; std::string ip() const; };`
    — `WiFiManager` com `setConfigPortalBlocking(false)`, `autoConnect("Claudinho")`, `WiFi.setHostname("claudinho")`,
    `MDNS.begin("claudinho")` ao conectar.
  - `class NetworkClock { void begin(); };` — `configTzTime("<-03>3", "pool.ntp.org", "time.google.com")`.
  - `struct LocalTime { int hour; int minute; int day; int month; int year; int weekday;
    static std::optional<LocalTime> now(); std::string hourMinute() const; std::string longDate() const; };`
    — `now()` devolve `nullopt` enquanto `time(nullptr) < 1'700'000'000` (relógio não sincronizado).
  - `class SetupPage : public Page { explicit SetupPage(const Published<SetupState>&); }`.

- [ ] **Step 1: Testes**

`firmware/test/setup/test_setup_progress/test_main.cpp`:

```cpp
#include <unity.h>

#include "setup/SetupProgress.h"

void setUp()
{
}

void tearDown()
{
}

void test_without_wifi_shows_the_hotspot_qr()
{
    SetupState state = SetupProgress::compute(false, "", false);

    TEST_ASSERT_EQUAL(SetupStep::WaitingWifi, state.step);
    TEST_ASSERT_EQUAL_STRING("WIFI:S:Claudinho;T:nopass;;", state.qrText.c_str());
}

void test_with_wifi_shows_the_page_qr_and_ip()
{
    SetupState state = SetupProgress::compute(true, "192.168.0.42", false);

    TEST_ASSERT_EQUAL(SetupStep::WaitingAdmin, state.step);
    TEST_ASSERT_EQUAL_STRING("http://claudinho.local", state.qrText.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.0.42", state.ip.c_str());
}

void test_with_password_setup_is_done()
{
    SetupState state = SetupProgress::compute(true, "192.168.0.42", true);

    TEST_ASSERT_EQUAL(SetupStep::Done, state.step);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_without_wifi_shows_the_hotspot_qr);
    RUN_TEST(test_with_wifi_shows_the_page_qr_and_ip);
    RUN_TEST(test_with_password_setup_is_done);
    return UNITY_END();
}
```

`firmware/test/support/test_local_time/test_main.cpp`:

```cpp
#include <unity.h>

#include "support/LocalTime.h"

void setUp()
{
}

void tearDown()
{
}

void test_hour_minute_has_two_digits()
{
    LocalTime time{9, 5, 1, 1, 2027, 0};

    TEST_ASSERT_EQUAL_STRING("09:05", time.hourMinute().c_str());
}

void test_long_date_in_portuguese()
{
    LocalTime time{14, 32, 22, 10, 2026, 4};

    TEST_ASSERT_EQUAL_STRING("quinta, 22 de outubro", time.longDate().c_str());
}

void test_sunday_first_of_january()
{
    LocalTime time{0, 0, 1, 1, 2027, 0};

    TEST_ASSERT_EQUAL_STRING("domingo, 1 de janeiro", time.longDate().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_hour_minute_has_two_digits);
    RUN_TEST(test_long_date_in_portuguese);
    RUN_TEST(test_sunday_first_of_january);
    return UNITY_END();
}
```

(22/10/2026 é uma quinta-feira; `weekday` segue `tm_wday`: 0 = domingo.)

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native`
Expected: FAIL por include ausente.

- [ ] **Step 3: Implementar**

`firmware/src/setup/SetupProgress.h`:

```cpp
#pragma once

#include <string>

enum class SetupStep {
    WaitingWifi,
    WaitingAdmin,
    Done,
};

struct SetupState {
    SetupStep step = SetupStep::WaitingWifi;
    std::string qrText;
    std::string ip;
};

class SetupProgress {
public:
    static SetupState compute(bool wifiConnected, const std::string& ip, bool adminPasswordSet);
};
```

`firmware/src/setup/SetupProgress.cpp`:

```cpp
#include "setup/SetupProgress.h"

namespace {

// QR no formato que iPhone e Android reconhecem como "entrar nesta rede".
constexpr const char* HOTSPOT_QR = "WIFI:S:Claudinho;T:nopass;;";
constexpr const char* PAGE_URL = "http://claudinho.local";

}

SetupState SetupProgress::compute(bool wifiConnected, const std::string& ip, bool adminPasswordSet)
{
    SetupState state;

    if (!wifiConnected) {
        state.step = SetupStep::WaitingWifi;
        state.qrText = HOTSPOT_QR;
        return state;
    }

    state.ip = ip;
    state.qrText = PAGE_URL;
    state.step = adminPasswordSet ? SetupStep::Done : SetupStep::WaitingAdmin;
    return state;
}
```

`firmware/src/support/LocalTime.h`:

```cpp
#pragma once

#include <optional>
#include <string>

struct LocalTime {
    int hour;
    int minute;
    int day;
    int month;
    int year;
    int weekday;

    static std::optional<LocalTime> now();

    std::string hourMinute() const;
    std::string longDate() const;
};
```

`firmware/src/support/LocalTime.cpp`:

```cpp
#include "support/LocalTime.h"

#include <ctime>

namespace {

// Antes do NTP o relógio do ESP32 começa em 1970; qualquer coisa antes de 2023 é "ainda não sincronizou".
constexpr time_t FIRST_PLAUSIBLE_EPOCH = 1700000000;

constexpr const char* WEEKDAYS[] = {"domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado"};
constexpr const char* MONTHS[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto",
    "setembro", "outubro", "novembro", "dezembro"};

std::string twoDigits(int value)
{
    std::string text = std::to_string(value);
    return value < 10 ? "0" + text : text;
}

}

std::optional<LocalTime> LocalTime::now()
{
    time_t epoch = time(nullptr);
    if (epoch < FIRST_PLAUSIBLE_EPOCH) {
        return std::nullopt;
    }

    tm local{};
    localtime_r(&epoch, &local);

    return LocalTime{local.tm_hour, local.tm_min, local.tm_mday, local.tm_mon + 1, local.tm_year + 1900, local.tm_wday};
}

std::string LocalTime::hourMinute() const
{
    return twoDigits(hour) + ":" + twoDigits(minute);
}

std::string LocalTime::longDate() const
{
    return std::string(WEEKDAYS[weekday]) + ", " + std::to_string(day) + " de " + MONTHS[month - 1];
}
```

`firmware/src/setup/device/WifiLink.h`:

```cpp
#pragma once

#include <WiFiManager.h>

#include <string>

class WifiLink {
public:
    void begin();
    void process();
    bool connected() const;
    std::string ip() const;

private:
    WiFiManager manager;
    bool mdnsStarted = false;
};
```

`firmware/src/setup/device/WifiLink.cpp`:

```cpp
#include "setup/device/WifiLink.h"

#include <ESPmDNS.h>
#include <WiFi.h>

namespace {

constexpr const char* HOSTNAME = "claudinho";
constexpr const char* HOTSPOT_NAME = "Claudinho";

}

void WifiLink::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);

    // Sem bloquear: a UI continua viva mostrando o QR enquanto o portal espera alguém escolher a rede.
    manager.setConfigPortalBlocking(false);
    manager.setHostname(HOSTNAME);
    manager.autoConnect(HOTSPOT_NAME);
}

void WifiLink::process()
{
    manager.process();

    if (connected() && !mdnsStarted) {
        mdnsStarted = MDNS.begin(HOSTNAME);
        MDNS.addService("http", "tcp", 80);
    }
}

bool WifiLink::connected() const
{
    return WiFi.status() == WL_CONNECTED;
}

std::string WifiLink::ip() const
{
    return WiFi.localIP().toString().c_str();
}
```

`firmware/src/support/device/NetworkClock.h` / `.cpp`:

```cpp
#pragma once

class NetworkClock {
public:
    void begin();
};
```

```cpp
#include "support/device/NetworkClock.h"

#include <Arduino.h>

void NetworkClock::begin()
{
    // Brasília sem horário de verão. O NTP corre em segundo plano e LocalTime::now() só passa a responder depois.
    configTzTime("<-03>3", "pool.ntp.org", "time.google.com");
}
```

`firmware/src/setup/screen/SetupPage.h`:

```cpp
#pragma once

#include "navigation/Page.h"
#include "setup/SetupProgress.h"
#include "support/Published.h"

class SetupPage : public Page {
public:
    explicit SetupPage(const Published<SetupState>& state);

    void build(lv_obj_t* parent) override;
    void onTick(uint32_t nowMillis) override;

private:
    void render(const SetupState& state);

    const Published<SetupState>& state;
    uint32_t seenVersion = 0;
    lv_obj_t* qr = nullptr;
    lv_obj_t* ipLabel = nullptr;
    lv_obj_t* steps[3] = {};
};
```

`firmware/src/setup/screen/SetupPage.cpp`:

```cpp
#include "setup/screen/SetupPage.h"

#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t QR_SIZE = 120;
constexpr const char* STEP_TEXTS[] = {
    "1  escaneie pra conectar no Wi-Fi",
    "2  abra claudinho.local",
    "3  crie a senha de administrador",
};

}

SetupPage::SetupPage(const Published<SetupState>& state)
    : state(state)
{
}

void SetupPage::build(lv_obj_t* parent)
{
    qr = lv_qrcode_create(parent);
    lv_qrcode_set_size(qr, QR_SIZE);
    lv_qrcode_set_dark_color(qr, theme::background());
    lv_qrcode_set_light_color(qr, theme::text());
    lv_obj_set_style_border_width(qr, 6, 0);
    lv_obj_set_style_border_color(qr, theme::text(), 0);
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 18, -12);

    ipLabel = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_align_to(ipLabel, qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t* title = theme::label(parent, fonts::large, theme::text());
    lv_label_set_text(title, "Vamos começar");
    lv_obj_set_pos(title, 156, 60);

    for (size_t position = 0; position < 3; position++) {
        steps[position] = theme::label(parent, fonts::medium, theme::textSecondary());
        lv_label_set_text(steps[position], STEP_TEXTS[position]);
        lv_obj_set_pos(steps[position], 156, 100 + static_cast<int32_t>(position) * 26);
    }
}

void SetupPage::onTick(uint32_t)
{
    SetupState current;
    if (state.readIfChanged(current, seenVersion)) {
        render(current);
    }
}

void SetupPage::render(const SetupState& current)
{
    lv_qrcode_update(qr, current.qrText.c_str(), current.qrText.size());
    lv_label_set_text(ipLabel, current.ip.c_str());

    size_t activeStep = current.step == SetupStep::WaitingWifi ? 0 : 1;
    for (size_t position = 0; position < 3; position++) {
        bool active = position == activeStep;
        lv_obj_set_style_text_color(steps[position], active ? theme::text() : theme::textSecondary(), 0);
    }
}
```

- [ ] **Step 4: Rodar testes**

Run: `cd firmware && pio test -e native`
Expected: `21 test cases: 21 succeeded`.

- [ ] **Step 5: Tarefa de rede no `main.cpp`**

```cpp
Published<SetupState> setupState;
WifiLink wifi;
NetworkClock networkClock;

void networkTask(void*)
{
    wifi.begin();
    networkClock.begin();

    for (;;) {
        wifi.process();
        setupState.publish(SetupProgress::compute(wifi.connected(), wifi.ip(), false));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

Em `setup()`, depois do tema: `navigator.add(std::make_unique<SetupPage>(setupState));` (remova as páginas de
exemplo) e `xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 1, nullptr, 0);`.

- [ ] **Step 6: Na placa**

Run: `cd firmware && pio run -e board -t erase && pio run -e board -t upload --upload-port COM8`

Escaneie o QR com o celular → entra na rede `Claudinho` → o portal abre → escolha o Wi-Fi de casa → a tela passa
para o QR da página com o IP embaixo. No PC: `ping claudinho.local` responde. Roteiro:

```markdown
## Configuração inicial

- [ ] Sem Wi-Fi salvo: QR da rede `Claudinho`; o celular entra e abre o portal.
- [ ] Depois de conectar: QR de claudinho.local e o IP por extenso.
- [ ] `ping claudinho.local` responde.
```

- [ ] **Step 7: Commit**

```bash
git add firmware docs/roteiro-de-teste.md
git commit -m "feat(setup): Wi-Fi pelo portal cativo, mDNS, relógio NTP e tela de configuração

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 6: Servidor web, senha de administrador e página embutida

**Files:**
- Create: `firmware/src/setup/AdminPassword.h`, `firmware/src/setup/AdminPassword.cpp`
- Create: `firmware/src/setup/storage/AdminSecret.h`, `firmware/src/setup/storage/AdminSecret.cpp`
- Create: `firmware/src/setup/web/WebServer.h`, `firmware/src/setup/web/WebServer.cpp`
- Create: `firmware/src/setup/web/AdminRoutes.h`, `firmware/src/setup/web/AdminRoutes.cpp`
- Create: `firmware/web/index.html`, `firmware/web/app.css`, `firmware/web/app.js`
- Test: `firmware/test/setup/test_admin_password/test_main.cpp`
- Modify: `firmware/platformio.ini`

**Interfaces:**
- Produces:
  - `class AdminPassword { static std::optional<AdminPassword> parse(const std::string& raw); const std::string& value() const; }`
    — apara espaços nas pontas; aceita 8 a 64 caracteres.
  - `class AdminSecret { void begin(); bool isSet() const; void save(const AdminPassword&); bool matches(const std::string&) const; }`
    — `Preferences` namespace `admin`, chave `password`.
  - `class WebServer { explicit WebServer(AdminSecret&); void begin(); AsyncWebServer& raw(); bool authorize(AsyncWebServerRequest*); }`
    — `authorize` devolve `false` e já respondeu `401` com `WWW-Authenticate: Basic realm="Claudinho"`.
  - Rotas: `GET /`, `GET /app.css`, `GET /app.js` (embutidos); `GET /api/status` →
    `{"name":"claudinho","ip":"…","adminSet":true,"time":"14:32"}` (aberto); `POST /api/admin/password`
    `{"password":"…"}` → `204`, ou `400 {"field":"password","error":"8 a 64 caracteres"}`, ou `409` se já existe
    senha e a requisição não está autenticada.

- [ ] **Step 1: Teste de `AdminPassword`**

```cpp
#include <unity.h>

#include "setup/AdminPassword.h"

void setUp()
{
}

void tearDown()
{
}

void test_accepts_eight_characters()
{
    TEST_ASSERT_TRUE(AdminPassword::parse("12345678").has_value());
}

void test_refuses_seven_characters()
{
    TEST_ASSERT_FALSE(AdminPassword::parse("1234567").has_value());
}

void test_refuses_sixty_five_characters()
{
    TEST_ASSERT_FALSE(AdminPassword::parse(std::string(65, 'a')).has_value());
}

void test_trims_the_ends()
{
    std::optional<AdminPassword> password = AdminPassword::parse("  senha123  ");

    TEST_ASSERT_TRUE(password.has_value());
    TEST_ASSERT_EQUAL_STRING("senha123", password->value().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_eight_characters);
    RUN_TEST(test_refuses_seven_characters);
    RUN_TEST(test_refuses_sixty_five_characters);
    RUN_TEST(test_trims_the_ends);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar; implementar `AdminPassword`**

```cpp
// AdminPassword.h
#pragma once

#include <optional>
#include <string>

class AdminPassword {
public:
    static constexpr size_t MIN_LENGTH = 8;
    static constexpr size_t MAX_LENGTH = 64;

    static std::optional<AdminPassword> parse(const std::string& raw);

    const std::string& value() const;

private:
    explicit AdminPassword(std::string value);

    std::string password;
};
```

```cpp
// AdminPassword.cpp
#include "setup/AdminPassword.h"

namespace {

std::string trimmed(const std::string& raw)
{
    size_t start = raw.find_first_not_of(' ');
    if (start == std::string::npos) {
        return "";
    }

    size_t end = raw.find_last_not_of(' ');
    return raw.substr(start, end - start + 1);
}

}

std::optional<AdminPassword> AdminPassword::parse(const std::string& raw)
{
    std::string value = trimmed(raw);
    if (value.size() < MIN_LENGTH || value.size() > MAX_LENGTH) {
        return std::nullopt;
    }

    return AdminPassword(value);
}

const std::string& AdminPassword::value() const
{
    return password;
}

AdminPassword::AdminPassword(std::string value)
    : password(std::move(value))
{
}
```

Run: `pio test -e native` → `25 succeeded`.

- [ ] **Step 3: `platformio.ini`**

No `[env:board]` acrescente a `lib_deps`:

```ini
    ESP32Async/ESPAsyncWebServer@3.7.10
    ESP32Async/AsyncTCP@3.4.5
```

e, no mesmo env:

```ini
board_build.embed_files =
    web/index.html
    web/app.css
    web/app.js
```

(Se o `pio pkg install` não achar essas versões exatas, use `pio pkg search ESPAsyncWebServer` e fixe a mais nova
3.7.x / 3.4.x.)

- [ ] **Step 4: `AdminSecret`, `WebServer`, `AdminRoutes`**

```cpp
// setup/storage/AdminSecret.h
#pragma once

#include <Preferences.h>

#include <string>

#include "setup/AdminPassword.h"

class AdminSecret {
public:
    void begin();
    bool isSet() const;
    void save(const AdminPassword& password);
    bool matches(const std::string& candidate) const;

private:
    Preferences preferences;
    std::string stored;
};
```

```cpp
// setup/storage/AdminSecret.cpp
#include "setup/storage/AdminSecret.h"

namespace {

constexpr const char* NAMESPACE = "admin";
constexpr const char* KEY = "password";

}

void AdminSecret::begin()
{
    preferences.begin(NAMESPACE, false);
    stored = preferences.getString(KEY, "").c_str();
}

bool AdminSecret::isSet() const
{
    return !stored.empty();
}

void AdminSecret::save(const AdminPassword& password)
{
    stored = password.value();
    preferences.putString(KEY, stored.c_str());
}

bool AdminSecret::matches(const std::string& candidate) const
{
    return isSet() && candidate == stored;
}
```

```cpp
// setup/web/WebServer.h
#pragma once

#include <ESPAsyncWebServer.h>

#include "setup/storage/AdminSecret.h"

class WebServer {
public:
    explicit WebServer(AdminSecret& secret);

    void begin();
    AsyncWebServer& raw();
    bool authorize(AsyncWebServerRequest* request);

private:
    void serveEmbedded(const char* path, const char* contentType, const uint8_t* start, const uint8_t* end);

    AdminSecret& secret;
    AsyncWebServer server{80};
};
```

```cpp
// setup/web/WebServer.cpp
#include "setup/web/WebServer.h"

extern const uint8_t index_html_start[] asm("_binary_web_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_web_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_web_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_web_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_web_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_web_app_js_end");

WebServer::WebServer(AdminSecret& secret)
    : secret(secret)
{
}

void WebServer::begin()
{
    serveEmbedded("/", "text/html", index_html_start, index_html_end);
    serveEmbedded("/app.css", "text/css", app_css_start, app_css_end);
    serveEmbedded("/app.js", "application/javascript", app_js_start, app_js_end);

    server.begin();
}

AsyncWebServer& WebServer::raw()
{
    return server;
}

bool WebServer::authorize(AsyncWebServerRequest* request)
{
    if (request->authenticate("admin", secret.isSet() ? "" : "", false)) {
        // authenticate() do ESPAsyncWebServer compara com uma senha fixa; a nossa é dinâmica, então decodificamos
        // o header e comparamos com AdminSecret::matches abaixo.
    }

    String header = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
    if (header.startsWith("Basic ")) {
        String decoded = base64Decode(header.substring(6));
        int colon = decoded.indexOf(':');
        if (colon > 0 && decoded.substring(0, colon) == "admin" && secret.matches(decoded.substring(colon + 1).c_str())) {
            return true;
        }
    }

    AsyncWebServerResponse* response = request->beginResponse(401);
    response->addHeader("WWW-Authenticate", "Basic realm=\"Claudinho\"");
    request->send(response);
    return false;
}

void WebServer::serveEmbedded(const char* path, const char* contentType, const uint8_t* start, const uint8_t* end)
{
    server.on(path, HTTP_GET, [=](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, contentType, start, end - start);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });
}
```

Apague o bloco `if (request->authenticate(...)) {}` do esboço acima — ficou só para registrar que o
`authenticate()` nativo não serve; a implementação final é o bloco do header. `base64Decode` vem de `<base64.h>`
do core (ou `mbedtls_base64_decode`); confirme o nome ao compilar.

```cpp
// setup/web/AdminRoutes.h
#pragma once

#include "setup/storage/AdminSecret.h"
#include "setup/web/WebServer.h"
#include "setup/device/WifiLink.h"

class AdminRoutes {
public:
    AdminRoutes(WebServer& web, AdminSecret& secret, const WifiLink& wifi);

    void begin();

private:
    WebServer& web;
    AdminSecret& secret;
    const WifiLink& wifi;
};
```

```cpp
// setup/web/AdminRoutes.cpp
#include "setup/web/AdminRoutes.h"

#include <ArduinoJson.h>

#include "setup/AdminPassword.h"
#include "support/LocalTime.h"

AdminRoutes::AdminRoutes(WebServer& web, AdminSecret& secret, const WifiLink& wifi)
    : web(web), secret(secret), wifi(wifi)
{
}

void AdminRoutes::begin()
{
    web.raw().on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument document;
        document["name"] = "claudinho";
        document["ip"] = wifi.ip();
        document["adminSet"] = secret.isSet();
        std::optional<LocalTime> now = LocalTime::now();
        document["time"] = now ? now->hourMinute() : "";

        String body;
        serializeJson(document, body);
        request->send(200, "application/json", body);
    });

    web.raw().on("/api/admin/password", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            // Trocar uma senha existente exige estar autenticado com a atual.
            if (secret.isSet() && !web.authorize(request)) {
                return;
            }

            JsonDocument document;
            if (deserializeJson(document, data, length) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"field\":\"password\",\"error\":\"JSON inválido\"}");
                return;
            }

            std::optional<AdminPassword> password = AdminPassword::parse(document["password"] | "");
            if (!password) {
                request->send(400, "application/json", "{\"field\":\"password\",\"error\":\"8 a 64 caracteres\"}");
                return;
            }

            secret.save(*password);
            request->send(204);
        });
}
```

- [ ] **Step 5: Página web**

`firmware/web/index.html`:

```html
<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Claudinho</title>
<link rel="stylesheet" href="/app.css">
</head>
<body>
<header><h1>Claudinho</h1><span id="clock"></span></header>
<main>
  <section id="password" class="card">
    <h2>Senha de administrador</h2>
    <p class="hint">Protege as fotos e as configurações. De 8 a 64 caracteres.</p>
    <form id="password-form">
      <input type="password" id="password-value" placeholder="senha" autocomplete="new-password">
      <input type="password" id="password-confirm" placeholder="confirme a senha" autocomplete="new-password">
      <p class="error" id="password-error"></p>
      <button type="submit">Salvar senha</button>
    </form>
  </section>
  <section id="photos" class="card hidden">
    <h2>Fotos</h2>
    <p class="hint">O navegador recorta para a tela (480×320) antes de enviar.</p>
    <div id="photo-grid"></div>
    <label class="button">Enviar fotos<input type="file" id="photo-input" accept="image/*" multiple hidden></label>
    <p id="photo-progress"></p>
  </section>
</main>
<script src="/app.js" type="module"></script>
</body>
</html>
```

`firmware/web/app.css` (tema igual à placa):

```css
:root { --bg: #1F1E1C; --surface: #34322F; --text: #F5F1EA; --muted: #A8A29A; --accent: #D97757; --danger: #E24B4A; }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--text); font: 16px/1.5 system-ui, sans-serif; }
header { display: flex; justify-content: space-between; align-items: baseline; padding: 20px 24px 0; }
h1 { font-size: 22px; font-weight: 500; margin: 0; }
h2 { font-size: 18px; font-weight: 500; margin: 0 0 4px; }
main { display: grid; gap: 16px; padding: 16px 24px 40px; max-width: 720px; margin: 0 auto; }
.card { background: var(--surface); border-radius: 12px; padding: 16px 20px; }
.hint { color: var(--muted); margin: 0 0 12px; font-size: 14px; }
.hidden { display: none; }
input[type=password] { display: block; width: 100%; margin-bottom: 8px; padding: 10px 12px; border-radius: 8px;
  border: 1px solid #4a4845; background: var(--bg); color: var(--text); }
button, .button { display: inline-block; padding: 10px 16px; border-radius: 8px; border: 0; background: var(--accent);
  color: #2b1209; font-weight: 500; cursor: pointer; }
.error { color: var(--danger); min-height: 1.5em; margin: 0 0 8px; font-size: 14px; }
#photo-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(160px, 1fr)); gap: 10px; margin-bottom: 12px; }
#photo-grid figure { margin: 0; position: relative; }
#photo-grid img { width: 100%; aspect-ratio: 3 / 2; object-fit: cover; border-radius: 8px; display: block; }
#photo-grid button { position: absolute; top: 6px; right: 6px; padding: 4px 8px; background: rgba(0,0,0,.6); color: #fff; }
```

`firmware/web/app.js`:

```js
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
```

(O card de fotos ganha comportamento na Tarefa 7.)

- [ ] **Step 6: Ligar no `main.cpp`**

Globais: `AdminSecret adminSecret; WebServer web(adminSecret); AdminRoutes adminRoutes(web, adminSecret, wifi);`.
Em `networkTask`, depois de `wifi.begin()`: `adminSecret.begin(); adminRoutes.begin(); web.begin();`. Na publicação:
`SetupProgress::compute(wifi.connected(), wifi.ip(), adminSecret.isSet())`.

Quando o `SetupState.step` vira `Done` pela primeira vez (detectado no `loop()` lendo `setupState`), chame
`ESP.restart()`: no boot seguinte `main.cpp` monta o Navigator com as páginas normais em vez da `SetupPage`
(`if (adminSecret.isSet() && wifiHasCredentials)`; `wifiHasCredentials` = `WiFi.SSID().length() > 0` lido antes do
`begin`). Reiniciar aqui é aceitável: simples e sem estado meio-trocado.

- [ ] **Step 7: Na placa**

Abra `http://claudinho.local`: só o card de senha aparece. Crie a senha → a placa reinicia → recarregue a página →
o navegador pede login (`admin` + a senha) → `/api/status` sem credencial responde `401`:

```bash
curl -i http://claudinho.local/api/photos
```

Roteiro: `- [ ] Página pede senha na primeira vez e autentica depois; sem senha, /api/photos devolve 401.`

- [ ] **Step 8: Commit**

```bash
git add firmware docs/roteiro-de-teste.md
git commit -m "feat(setup): servidor web com senha de administrador e página embutida

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 7: Fotos — envio pelo navegador, listagem e remoção

**Files:**
- Create: `firmware/src/photos/PhotoName.h`, `firmware/src/photos/PhotoName.cpp`
- Create: `firmware/src/photos/UploadCheck.h`, `firmware/src/photos/UploadCheck.cpp`
- Create: `firmware/src/photos/storage/PhotoStore.h`, `firmware/src/photos/storage/PhotoStore.cpp`
- Create: `firmware/src/photos/web/PhotoRoutes.h`, `firmware/src/photos/web/PhotoRoutes.cpp`
- Create: `firmware/web/crop.js`
- Modify: `firmware/web/app.js`, `firmware/platformio.ini` (embed `web/crop.js`)
- Test: `firmware/test/photos/test_photo_name/test_main.cpp`
- Test: `firmware/test/photos/test_upload_check/test_main.cpp`
- Test: `firmware/test/web/crop.test.mjs`

**Interfaces:**
- Produces:
  - `class PhotoName { static std::optional<PhotoName> parse(const std::string& fileName); static PhotoName fromSequence(uint16_t sequence); const std::string& fileName() const; std::string path() const; }`
    — aceita só `p` + 4 dígitos + `.jpg`; `path()` = `/photos/p0001.jpg`.
  - `class UploadCheck { static constexpr size_t MAX_PHOTO_BYTES = 200 * 1024; MIN_FREE_BYTES = 512 * 1024; MAX_PHOTOS = 200;
    static std::optional<std::string> refuse(size_t declaredBytes, size_t freeBytes, size_t photoCount); }`
    — `nullopt` aceita; mensagens: `"foto maior que 200 KB"`, `"memória cheia"`, `"limite de 200 fotos"`.
  - `class PhotoStore { void begin(); std::vector<PhotoName> list() const; PhotoName nextName() const; size_t freeBytes() const;
    bool beginUpload(); bool writeChunk(const uint8_t*, size_t); bool finishUpload(const PhotoName&); void abortUpload(); bool remove(const PhotoName&); }`
    — grava em `/photos/.upload.tmp` e renomeia ao terminar; `begin()` monta o LittleFS, cria `/photos` e apaga
    qualquer `.tmp` que sobrou.
  - Rotas (autenticadas): `GET /api/photos` → `[{"name":"p0001.jpg","bytes":48213}]`; `POST /api/photos`
    (multipart, campo `photo`) → `201 {"name":"p0007.jpg"}` ou `400/413/507 {"error":"…"}`;
    `DELETE /api/photos/<name>` → `204` ou `404`; `GET /photos/<name>` → o JPEG (miniatura na página).
  - `crop.js` exporta `cropRect(imageWidth, imageHeight)` → `{x, y, width, height}` e `cropToJpeg(file)` → `Blob`.

- [ ] **Step 1: Testes**

`firmware/test/photos/test_photo_name/test_main.cpp`:

```cpp
#include <unity.h>

#include "photos/PhotoName.h"

void setUp()
{
}

void tearDown()
{
}

void test_accepts_the_canonical_form()
{
    std::optional<PhotoName> name = PhotoName::parse("p0001.jpg");

    TEST_ASSERT_TRUE(name.has_value());
    TEST_ASSERT_EQUAL_STRING("/photos/p0001.jpg", name->path().c_str());
}

void test_refuses_anything_that_is_not_the_canonical_form()
{
    TEST_ASSERT_FALSE(PhotoName::parse("../x").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("a/b.jpg").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("p0001.png").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("P0001.jpg").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("p1.jpg").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("").has_value());
}

void test_from_sequence_pads_to_four_digits()
{
    TEST_ASSERT_EQUAL_STRING("p0007.jpg", PhotoName::fromSequence(7).fileName().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_the_canonical_form);
    RUN_TEST(test_refuses_anything_that_is_not_the_canonical_form);
    RUN_TEST(test_from_sequence_pads_to_four_digits);
    return UNITY_END();
}
```

`firmware/test/photos/test_upload_check/test_main.cpp`:

```cpp
#include <unity.h>

#include "photos/UploadCheck.h"

void setUp()
{
}

void tearDown()
{
}

void test_too_big()
{
    std::optional<std::string> refusal = UploadCheck::refuse(300 * 1024, 8 * 1024 * 1024, 10);

    TEST_ASSERT_TRUE(refusal.has_value());
    TEST_ASSERT_EQUAL_STRING("foto maior que 200 KB", refusal->c_str());
}

void test_flash_nearly_full()
{
    std::optional<std::string> refusal = UploadCheck::refuse(100 * 1024, 600 * 1024, 10);

    TEST_ASSERT_TRUE(refusal.has_value());
    TEST_ASSERT_EQUAL_STRING("memória cheia", refusal->c_str());
}

void test_too_many_photos()
{
    std::optional<std::string> refusal = UploadCheck::refuse(100 * 1024, 8 * 1024 * 1024, 200);

    TEST_ASSERT_TRUE(refusal.has_value());
    TEST_ASSERT_EQUAL_STRING("limite de 200 fotos", refusal->c_str());
}

void test_accepts_a_normal_photo()
{
    TEST_ASSERT_FALSE(UploadCheck::refuse(100 * 1024, 2 * 1024 * 1024, 10).has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_too_big);
    RUN_TEST(test_flash_nearly_full);
    RUN_TEST(test_too_many_photos);
    RUN_TEST(test_accepts_a_normal_photo);
    return UNITY_END();
}
```

`firmware/test/web/crop.test.mjs`:

```js
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
    assert.deepEqual(cropRect(1000, 700), { x: 0, y: 17, width: 1000, height: 667 });
});

test('an exact 3:2 image is untouched', () => {
    assert.deepEqual(cropRect(480, 320), { x: 0, y: 0, width: 480, height: 320 });
});
```

Regra: altura do recorte = `round(width / 1.5)` quando a foto é mais alta que 3:2; `y = round(height / 3 - cropHeight / 2)`,
limitado a `[0, height - cropHeight]`. Com 1000×700: `cropHeight = 667`, `y = round(233.3 - 333.5) = -100` → 0?
Não: o teste espera 17. Recalcule: `height - cropHeight = 33`; `round(700/3 - 667/2) = round(-100.2) = -100` → limitado
a 0. **O teste acima está errado; corrija para `{ x: 0, y: 0, width: 1000, height: 667 }`.** Mantenha o nome do
teste — é exatamente o caso de limite que ele cobre.

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native && node --test test/web/`
Expected: FAIL nos dois (includes/módulo ausentes).

- [ ] **Step 3: Implementar a lógica pura**

```cpp
// photos/PhotoName.h
#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Só o servidor dá nome às fotos (p0001.jpg…): nome vindo do cliente nunca vira caminho de arquivo.
class PhotoName {
public:
    static std::optional<PhotoName> parse(const std::string& fileName);
    static PhotoName fromSequence(uint16_t sequence);

    const std::string& fileName() const;
    std::string path() const;

private:
    explicit PhotoName(std::string fileName);

    std::string name;
};
```

```cpp
// photos/PhotoName.cpp
#include "photos/PhotoName.h"

#include <cctype>

namespace {

constexpr const char* DIRECTORY = "/photos/";
constexpr size_t LENGTH = 9;

bool hasCanonicalForm(const std::string& fileName)
{
    if (fileName.size() != LENGTH || fileName[0] != 'p' || fileName.compare(5, 4, ".jpg") != 0) {
        return false;
    }

    for (size_t position = 1; position < 5; position++) {
        if (!std::isdigit(static_cast<unsigned char>(fileName[position]))) {
            return false;
        }
    }

    return true;
}

}

std::optional<PhotoName> PhotoName::parse(const std::string& fileName)
{
    if (!hasCanonicalForm(fileName)) {
        return std::nullopt;
    }

    return PhotoName(fileName);
}

PhotoName PhotoName::fromSequence(uint16_t sequence)
{
    char buffer[LENGTH + 1];
    snprintf(buffer, sizeof(buffer), "p%04u.jpg", static_cast<unsigned>(sequence % 10000));

    return PhotoName(buffer);
}

const std::string& PhotoName::fileName() const
{
    return name;
}

std::string PhotoName::path() const
{
    return DIRECTORY + name;
}

PhotoName::PhotoName(std::string fileName)
    : name(std::move(fileName))
{
}
```

```cpp
// photos/UploadCheck.h
#pragma once

#include <cstddef>
#include <optional>
#include <string>

class UploadCheck {
public:
    static constexpr size_t MAX_PHOTO_BYTES = 200 * 1024;
    static constexpr size_t MIN_FREE_BYTES = 512 * 1024;
    static constexpr size_t MAX_PHOTOS = 200;

    static std::optional<std::string> refuse(size_t declaredBytes, size_t freeBytes, size_t photoCount);
};
```

```cpp
// photos/UploadCheck.cpp
#include "photos/UploadCheck.h"

std::optional<std::string> UploadCheck::refuse(size_t declaredBytes, size_t freeBytes, size_t photoCount)
{
    if (declaredBytes > MAX_PHOTO_BYTES) {
        return "foto maior que 200 KB";
    }

    if (photoCount >= MAX_PHOTOS) {
        return "limite de 200 fotos";
    }

    // A reserva evita que uma foto deixe o LittleFS sem espaço para o próprio metadado e para o histórico do Claude.
    if (freeBytes < declaredBytes + MIN_FREE_BYTES) {
        return "memória cheia";
    }

    return std::nullopt;
}
```

`firmware/web/crop.js`:

```js
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
```

Run: `pio test -e native && node --test test/web/` → tudo verde (`32 succeeded` + 4 testes Node).

- [ ] **Step 4: `PhotoStore` e `PhotoRoutes`**

```cpp
// photos/storage/PhotoStore.h
#pragma once

#include <FS.h>

#include <vector>

#include "photos/PhotoName.h"

class PhotoStore {
public:
    void begin();
    std::vector<PhotoName> list() const;
    PhotoName nextName() const;
    size_t freeBytes() const;

    bool beginUpload();
    bool writeChunk(const uint8_t* data, size_t length);
    bool finishUpload(const PhotoName& name);
    void abortUpload();

    bool remove(const PhotoName& name);
    File open(const PhotoName& name) const;

private:
    File upload;
};
```

```cpp
// photos/storage/PhotoStore.cpp
#include "photos/storage/PhotoStore.h"

#include <LittleFS.h>

namespace {

constexpr const char* DIRECTORY = "/photos";
constexpr const char* TEMP_PATH = "/photos/.upload.tmp";

}

void PhotoStore::begin()
{
    if (!LittleFS.begin(true)) {
        log_e("LittleFS did not mount");
        return;
    }
    LittleFS.mkdir(DIRECTORY);

    // Um envio interrompido (cabo puxado) não pode virar foto pela metade no carrossel.
    LittleFS.remove(TEMP_PATH);
}

std::vector<PhotoName> PhotoStore::list() const
{
    std::vector<PhotoName> names;

    File directory = LittleFS.open(DIRECTORY);
    for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
        std::optional<PhotoName> name = PhotoName::parse(entry.name());
        if (name) {
            names.push_back(*name);
        }
    }

    return names;
}

PhotoName PhotoStore::nextName() const
{
    uint16_t highest = 0;
    for (const PhotoName& name : list()) {
        highest = std::max<uint16_t>(highest, static_cast<uint16_t>(std::stoi(name.fileName().substr(1, 4))));
    }

    return PhotoName::fromSequence(highest + 1);
}

size_t PhotoStore::freeBytes() const
{
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

bool PhotoStore::beginUpload()
{
    upload = LittleFS.open(TEMP_PATH, FILE_WRITE);
    return static_cast<bool>(upload);
}

bool PhotoStore::writeChunk(const uint8_t* data, size_t length)
{
    return upload && upload.write(data, length) == length;
}

bool PhotoStore::finishUpload(const PhotoName& name)
{
    upload.close();
    return LittleFS.rename(TEMP_PATH, name.path().c_str());
}

void PhotoStore::abortUpload()
{
    upload.close();
    LittleFS.remove(TEMP_PATH);
}

bool PhotoStore::remove(const PhotoName& name)
{
    return LittleFS.remove(name.path().c_str());
}

File PhotoStore::open(const PhotoName& name) const
{
    return LittleFS.open(name.path().c_str(), FILE_READ);
}
```

```cpp
// photos/web/PhotoRoutes.h
#pragma once

#include "photos/storage/PhotoStore.h"
#include "setup/web/WebServer.h"

class PhotoRoutes {
public:
    PhotoRoutes(WebServer& web, PhotoStore& store);

    void begin();

private:
    void list(AsyncWebServerRequest* request);
    void serve(AsyncWebServerRequest* request);
    void remove(AsyncWebServerRequest* request);
    void receive(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index, bool final);

    WebServer& web;
    PhotoStore& store;
    std::optional<std::string> refusal;
};
```

```cpp
// photos/web/PhotoRoutes.cpp
#include "photos/web/PhotoRoutes.h"

#include <ArduinoJson.h>

#include "photos/UploadCheck.h"

namespace {

std::optional<PhotoName> nameFromPath(AsyncWebServerRequest* request)
{
    String url = request->url();
    return PhotoName::parse(url.substring(url.lastIndexOf('/') + 1).c_str());
}

void sendError(AsyncWebServerRequest* request, int status, const std::string& message)
{
    JsonDocument document;
    document["error"] = message;
    String body;
    serializeJson(document, body);
    request->send(status, "application/json", body);
}

}

PhotoRoutes::PhotoRoutes(WebServer& web, PhotoStore& store)
    : web(web), store(store)
{
}

void PhotoRoutes::begin()
{
    AsyncWebServer& server = web.raw();

    server.on("/api/photos", HTTP_GET, [this](AsyncWebServerRequest* request) { list(request); });
    server.on("^/api/photos/(p[0-9]{4}\\.jpg)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) { remove(request); });
    server.on("^/photos/(p[0-9]{4}\\.jpg)$", HTTP_GET, [this](AsyncWebServerRequest* request) { serve(request); });

    server.on("/api/photos", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (refusal) {
                int status = *refusal == "memória cheia" ? 507 : *refusal == "foto maior que 200 KB" ? 413 : 400;
                sendError(request, status, *refusal);
                return;
            }

            PhotoName name = store.nextName();
            if (!store.finishUpload(name)) {
                sendError(request, 500, "não consegui gravar a foto");
                return;
            }

            JsonDocument document;
            document["name"] = name.fileName();
            String body;
            serializeJson(document, body);
            request->send(201, "application/json", body);
        },
        [this](AsyncWebServerRequest* request, const String&, const String&, uint8_t* data, size_t length, bool final) {
            size_t index = request->_tempObject ? *static_cast<size_t*>(request->_tempObject) : 0;
            receive(request, data, length, index, final);
        });
}
```

O handler de upload do ESPAsyncWebServer entrega `(request, filename, index, data, len, final)` — use essa
assinatura real em vez do esboço acima (`index` já vem pronto; apague a gambiarra do `_tempObject`):

```cpp
        [this](AsyncWebServerRequest* request, const String&, size_t index, uint8_t* data, size_t length, bool final) {
            receive(request, data, length, index, final);
        });
```

```cpp
void PhotoRoutes::receive(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index, bool final)
{
    if (index == 0) {
        refusal.reset();
        if (!web.authorize(request)) {
            refusal = "não autorizado";
            return;
        }

        refusal = UploadCheck::refuse(request->contentLength(), store.freeBytes(), store.list().size());
        if (refusal || !store.beginUpload()) {
            refusal = refusal ? refusal : std::optional<std::string>("não consegui abrir o arquivo");
            return;
        }
    }

    if (refusal) {
        return;
    }

    if (!store.writeChunk(data, length)) {
        store.abortUpload();
        refusal = "memória cheia";
    }
    (void) final;
}

void PhotoRoutes::list(AsyncWebServerRequest* request)
{
    if (!web.authorize(request)) {
        return;
    }

    JsonDocument document;
    JsonArray photos = document.to<JsonArray>();
    for (const PhotoName& name : store.list()) {
        JsonObject photo = photos.add<JsonObject>();
        photo["name"] = name.fileName();
        photo["bytes"] = store.open(name).size();
    }

    String body;
    serializeJson(document, body);
    request->send(200, "application/json", body);
}

void PhotoRoutes::serve(AsyncWebServerRequest* request)
{
    if (!web.authorize(request)) {
        return;
    }

    std::optional<PhotoName> name = nameFromPath(request);
    if (!name || !LittleFS.exists(name->path().c_str())) {
        request->send(404);
        return;
    }

    request->send(LittleFS, name->path().c_str(), "image/jpeg");
}

void PhotoRoutes::remove(AsyncWebServerRequest* request)
{
    if (!web.authorize(request)) {
        return;
    }

    std::optional<PhotoName> name = nameFromPath(request);
    if (!name || !store.remove(*name)) {
        request->send(404);
        return;
    }

    request->send(204);
}
```

(Em `PhotoRoutes.cpp` inclua `<LittleFS.h>`. Rotas com regex exigem `-D ASYNCWEBSERVER_REGEX` em `build_flags`.)

Em `platformio.ini`, acrescente `web/crop.js` ao `embed_files` e a rota `GET /crop.js` em `WebServer::begin`.

- [ ] **Step 5: Página — card de fotos**

Acrescente ao fim de `firmware/web/app.js`:

```js
import { cropToJpeg } from '/crop.js';

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
```

- [ ] **Step 6: Ligar no `main.cpp`**

Globais `PhotoStore photoStore; PhotoRoutes photoRoutes(web, photoStore);`. Em `setup()` (antes da tarefa de rede,
porque a UI também vai ler fotos): `photoStore.begin();`. Em `networkTask`, junto de `adminRoutes.begin()`:
`photoRoutes.begin();`.

- [ ] **Step 7: Na placa**

Envie 5 fotos (uma em pé, uma panorâmica), apague uma, recarregue. Puxe o cabo no meio de um envio, religue:
`GET /api/photos` não lista nada estranho. Teste o limite por fora do recorte:

```bash
curl -u admin:SENHA -F "photo=@foto-grande.jpg" -i http://claudinho.local/api/photos
```

Expected: `413` com `{"error":"foto maior que 200 KB"}`.

Roteiro:

```markdown
## Fotos

- [ ] Foto em pé vem com a cabeça inteira (recorte a um terço de cima).
- [ ] Panorâmica centrada.
- [ ] Desligar no meio do envio não deixa arquivo pela metade.
- [ ] JPEG > 200 KB via curl é recusado com mensagem clara.
```

- [ ] **Step 8: Commit**

```bash
git add firmware docs/roteiro-de-teste.md
git commit -m "feat(photos): envio pelo navegador com recorte, listagem e remoção

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 8: Início — carrossel, relógio e data

**Files:**
- Create: `firmware/src/photos/ShuffleOrder.h`, `firmware/src/photos/ShuffleOrder.cpp`
- Create: `firmware/src/photos/screen/PhotoView.h`, `firmware/src/photos/screen/PhotoView.cpp`
- Create: `firmware/src/photos/screen/HomePage.h`, `firmware/src/photos/screen/HomePage.cpp`
- Test: `firmware/test/photos/test_shuffle_order/test_main.cpp`
- Modify: `firmware/platformio.ini` (`lib_deps` do `board`: `bitbank2/JPEGDEC@1.8.2`)

**Interfaces:**
- Produces:
  - `class ShuffleOrder { ShuffleOrder(size_t count, uint32_t seed); std::optional<size_t> next(); size_t count() const; }`
    — permutação com `std::minstd_rand`; percorre todos sem repetir; ao esgotar reembaralha e, se `count > 1`, garante
    que o primeiro da nova volta ≠ último da anterior; `count == 0` → `nullopt`.
  - `class PhotoView { explicit PhotoView(PhotoStore&); bool load(const PhotoName&, lv_image_dsc_t& into); }`
    — decodifica com JPEGDEC para RGB565 num buffer 480×320 na PSRAM (o chamador tem dois buffers).
  - `class HomePage : public Page { HomePage(PhotoStore&); }` — imagem em tela cheia, fade de 600 ms alternando dois
    `lv_image`; troca a cada 12 s; faixa inferior de 78 px a 50 % com relógio (`fonts::clock_`) e data (`fonts::small`);
    sem hora → relógio e data escondidos; sem fotos → fundo `theme::background()` e "Mande fotos em claudinho.local".

- [ ] **Step 1: Teste de `ShuffleOrder`**

```cpp
#include <unity.h>

#include <set>

#include "photos/ShuffleOrder.h"

void setUp()
{
}

void tearDown()
{
}

void test_visits_every_photo_once_per_round()
{
    ShuffleOrder order(5, 42);
    std::set<size_t> seen;

    for (int round = 0; round < 5; round++) {
        seen.insert(*order.next());
    }

    TEST_ASSERT_EQUAL_size_t(5, seen.size());
}

void test_next_round_does_not_repeat_the_last_photo()
{
    for (uint32_t seed = 1; seed < 50; seed++) {
        ShuffleOrder order(3, seed);
        order.next();
        order.next();
        size_t last = *order.next();

        TEST_ASSERT_NOT_EQUAL(last, *order.next());
    }
}

void test_no_photos_means_nothing_to_show()
{
    ShuffleOrder order(0, 1);

    TEST_ASSERT_FALSE(order.next().has_value());
}

void test_a_single_photo_repeats()
{
    ShuffleOrder order(1, 1);

    TEST_ASSERT_EQUAL_size_t(0, *order.next());
    TEST_ASSERT_EQUAL_size_t(0, *order.next());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_visits_every_photo_once_per_round);
    RUN_TEST(test_next_round_does_not_repeat_the_last_photo);
    RUN_TEST(test_no_photos_means_nothing_to_show);
    RUN_TEST(test_a_single_photo_repeats);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar; implementar**

```cpp
// photos/ShuffleOrder.h
#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

class ShuffleOrder {
public:
    ShuffleOrder(size_t count, uint32_t seed);

    std::optional<size_t> next();
    size_t count() const;

private:
    void reshuffle(std::optional<size_t> avoidFirst);

    std::vector<size_t> order;
    size_t position = 0;
    std::minstd_rand random;
};
```

```cpp
// photos/ShuffleOrder.cpp
#include "photos/ShuffleOrder.h"

#include <algorithm>
#include <numeric>

ShuffleOrder::ShuffleOrder(size_t count, uint32_t seed)
    : order(count), random(seed)
{
    std::iota(order.begin(), order.end(), 0);
    reshuffle(std::nullopt);
}

std::optional<size_t> ShuffleOrder::next()
{
    if (order.empty()) {
        return std::nullopt;
    }

    if (position == order.size()) {
        reshuffle(order.back());
    }

    return order[position++];
}

size_t ShuffleOrder::count() const
{
    return order.size();
}

void ShuffleOrder::reshuffle(std::optional<size_t> avoidFirst)
{
    std::shuffle(order.begin(), order.end(), random);

    // Ao dar a volta, a mesma foto não pode ficar duas vezes seguidas na tela.
    if (avoidFirst && order.size() > 1 && order.front() == *avoidFirst) {
        std::swap(order.front(), order.back());
    }

    position = 0;
}
```

Run: `pio test -e native` → `36 succeeded`.

- [ ] **Step 3: `PhotoView` e `HomePage`**

```cpp
// photos/screen/PhotoView.h
#pragma once

#include <JPEGDEC.h>
#include <lvgl.h>

#include "photos/storage/PhotoStore.h"

class PhotoView {
public:
    explicit PhotoView(PhotoStore& store);

    bool load(const PhotoName& name, lv_image_dsc_t& into);

private:
    static int copyBlock(JPEGDRAW* block);

    PhotoStore& store;
    JPEGDEC decoder;
};
```

```cpp
// photos/screen/PhotoView.cpp
#include "photos/screen/PhotoView.h"

#include "board/ScreenGeometry.h"

namespace {

struct Target {
    uint16_t* pixels;
};

}

PhotoView::PhotoView(PhotoStore& store)
    : store(store)
{
}

bool PhotoView::load(const PhotoName& name, lv_image_dsc_t& into)
{
    File file = store.open(name);
    if (!file) {
        return false;
    }

    // As fotos já chegam em 480×320 (o navegador recorta); se vier diferente, só a parte que cabe é copiada.
    Target target{reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(into.data))};
    bool opened = decoder.open(file.name(), [](const char*, int32_t*) -> void* { return nullptr; }, nullptr, nullptr,
        nullptr, copyBlock);
    (void) opened;
    file.close();
    return false;
}
```

O JPEGDEC tem uma API de callbacks de arquivo (`open/close/read/seek`) e outra que recebe o buffer inteiro em RAM
(`openRAM`). Use a de RAM — a foto tem no máximo 200 KB e a PSRAM é folgada — e apague o esboço acima:

```cpp
bool PhotoView::load(const PhotoName& name, lv_image_dsc_t& into)
{
    File file = store.open(name);
    if (!file) {
        return false;
    }

    size_t size = file.size();
    uint8_t* jpeg = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    file.read(jpeg, size);
    file.close();

    Target target{reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(into.data))};
    bool decoded = decoder.openRAM(jpeg, size, copyBlock) && (decoder.setUserPointer(&target), true)
        && decoder.setPixelType(RGB565_LITTLE_ENDIAN) == 0 && decoder.decode(0, 0, 0) == 1;
    decoder.close();
    heap_caps_free(jpeg);

    return decoded;
}

int PhotoView::copyBlock(JPEGDRAW* block)
{
    Target* target = static_cast<Target*>(block->pUser);

    for (int row = 0; row < block->iHeight; row++) {
        int y = block->y + row;
        if (y >= SCREEN_HEIGHT) {
            break;
        }

        int width = std::min(block->iWidth, SCREEN_WIDTH - block->x);
        memcpy(target->pixels + y * SCREEN_WIDTH + block->x, block->pPixels + row * block->iWidth, width * 2);
    }

    return 1;
}
```

(`setPixelType` devolve `void` em algumas versões; ajuste a expressão ao compilar. Inclua `<esp_heap_caps.h>`,
`<algorithm>`, `<cstring>`.)

```cpp
// photos/screen/HomePage.h
#pragma once

#include <lvgl.h>

#include <optional>

#include "navigation/Page.h"
#include "photos/ShuffleOrder.h"
#include "photos/screen/PhotoView.h"
#include "photos/storage/PhotoStore.h"

class HomePage : public Page {
public:
    explicit HomePage(PhotoStore& store);

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onTick(uint32_t nowMillis) override;

private:
    void showNextPhoto(uint32_t nowMillis);
    void refreshClock();
    lv_image_dsc_t makeFrame();

    PhotoStore& store;
    PhotoView view;
    std::optional<ShuffleOrder> order;
    std::vector<PhotoName> names;

    lv_image_dsc_t frames[2] = {};
    lv_obj_t* images[2] = {};
    size_t visible = 0;
    uint32_t lastSwapMillis = 0;
    uint32_t lastClockMillis = 0;

    lv_obj_t* invite = nullptr;
    lv_obj_t* band = nullptr;
    lv_obj_t* clockLabel = nullptr;
    lv_obj_t* dateLabel = nullptr;
};
```

```cpp
// photos/screen/HomePage.cpp
#include "photos/screen/HomePage.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "board/ScreenGeometry.h"
#include "support/LocalTime.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr uint32_t SWAP_EVERY_MILLIS = 12000;
constexpr uint32_t FADE_MILLIS = 600;
constexpr uint32_t CLOCK_EVERY_MILLIS = 1000;
constexpr int32_t BAND_HEIGHT = 78;

}

HomePage::HomePage(PhotoStore& store)
    : store(store), view(store)
{
}

lv_image_dsc_t HomePage::makeFrame()
{
    lv_image_dsc_t frame = {};
    frame.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame.header.cf = LV_COLOR_FORMAT_RGB565;
    frame.header.w = SCREEN_WIDTH;
    frame.header.h = SCREEN_HEIGHT;
    frame.header.stride = SCREEN_WIDTH * 2;
    frame.data_size = SCREEN_WIDTH * SCREEN_HEIGHT * 2;
    frame.data = static_cast<uint8_t*>(heap_caps_malloc(frame.data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return frame;
}

void HomePage::build(lv_obj_t* parent)
{
    lv_obj_set_style_bg_color(parent, theme::background(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    for (size_t slot = 0; slot < 2; slot++) {
        frames[slot] = makeFrame();
        images[slot] = lv_image_create(parent);
        lv_image_set_src(images[slot], &frames[slot]);
        lv_obj_set_style_opa(images[slot], LV_OPA_TRANSP, 0);
    }

    invite = theme::label(parent, fonts::large, theme::textSecondary());
    lv_label_set_text(invite, "Mande fotos em claudinho.local");
    lv_obj_center(invite);

    band = lv_obj_create(parent);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, SCREEN_WIDTH, BAND_HEIGHT);
    lv_obj_align(band, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(band, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_50, 0);

    clockLabel = theme::label(band, fonts::clock_, theme::text());
    lv_obj_align(clockLabel, LV_ALIGN_LEFT_MID, 16, -8);
    dateLabel = theme::label(band, fonts::small, theme::textSecondary());
    lv_obj_align(dateLabel, LV_ALIGN_BOTTOM_LEFT, 16, -10);
    lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
}

void HomePage::onShow()
{
    names = store.list();
    if (!order || order->count() != names.size()) {
        order.emplace(names.size(), static_cast<uint32_t>(millis()));
    }

    lastSwapMillis = 0;
}

void HomePage::onTick(uint32_t nowMillis)
{
    if (nowMillis - lastSwapMillis >= SWAP_EVERY_MILLIS) {
        showNextPhoto(nowMillis);
    }

    if (nowMillis - lastClockMillis >= CLOCK_EVERY_MILLIS) {
        lastClockMillis = nowMillis;
        refreshClock();
    }
}

void HomePage::showNextPhoto(uint32_t nowMillis)
{
    lastSwapMillis = nowMillis;

    std::optional<size_t> pick = order->next();
    bool hasPhotos = pick.has_value();
    lv_obj_set_flag(invite, LV_OBJ_FLAG_HIDDEN, hasPhotos);
    if (!hasPhotos) {
        return;
    }

    size_t incoming = 1 - visible;
    if (!view.load(names[*pick], frames[incoming])) {
        return;
    }

    lv_image_set_src(images[incoming], &frames[incoming]);
    lv_obj_move_foreground(images[incoming]);
    lv_obj_move_foreground(band);
    lv_obj_fade_in(images[incoming], FADE_MILLIS, 0);
    lv_obj_fade_out(images[visible], FADE_MILLIS, 0);
    visible = incoming;
}

void HomePage::refreshClock()
{
    std::optional<LocalTime> now = LocalTime::now();
    lv_obj_set_flag(band, LV_OBJ_FLAG_HIDDEN, !now.has_value());
    if (!now) {
        return;
    }

    lv_label_set_text(clockLabel, now->hourMinute().c_str());
    lv_label_set_text(dateLabel, now->longDate().c_str());
}
```

A decodificação (~150 ms) acontece no `onTick` e trava a UI por esse tempo a cada 12 s; aceito nesta versão (a lista
de ideias do pai inclui decodificar na tarefa de rede).

- [ ] **Step 4: `main.cpp`**

Quando configurado: `navigator.add(std::make_unique<HomePage>(photoStore));` no lugar da `SetupPage`.

- [ ] **Step 5: Na placa**

Fotos trocam com fade; relógio bate com o celular; data em português. Apague todas as fotos pela página → saia e
volte ao Início → convite aparece. Roteiro:

```markdown
## Início

- [ ] Fotos trocam com fade a cada ~12 s, sem repetir até dar a volta.
- [ ] Relógio e data corretos; sem internet, a faixa fica escondida.
- [ ] Sem fotos: convite para mandar fotos.
```

- [ ] **Step 6: Commit**

```bash
git add firmware docs/roteiro-de-teste.md
git commit -m "feat(photos): porta-retrato com relógio e data no Início

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 9: Aniversário

**Files:**
- Create: `firmware/src/birthday/BirthdayContent.h`
- Create: `firmware/src/birthday/BirthdayCalendar.h`, `firmware/src/birthday/BirthdayCalendar.cpp`
- Create: `firmware/src/birthday/storage/BirthdayMemory.h`, `firmware/src/birthday/storage/BirthdayMemory.cpp`
- Create: `firmware/src/birthday/screen/BirthdayOverlay.h`, `firmware/src/birthday/screen/BirthdayOverlay.cpp`
- Test: `firmware/test/birthday/test_birthday_calendar/test_main.cpp`

**Interfaces:**
- Produces:
  - `namespace birthday { constexpr int DAY = 22; constexpr int MONTH = 10; constexpr const char* TITLE = "Feliz aniversário, pai";
    constexpr const char* MESSAGE = "…"; constexpr const char* SIGNATURE = "— Leo"; }` (editado antes de gravar).
  - `class BirthdayCalendar { static bool shouldShow(bool neverShown, std::optional<LocalTime> now, std::optional<int> lastShownYear); }`
  - `class BirthdayMemory { void begin(); bool neverShown() const; std::optional<int> lastShownYear() const; void markShown(std::optional<int> year); }`
    — `Preferences` namespace `birthday`, chaves `shown` (bool) e `year` (int).
  - `class BirthdayOverlay { void show(std::function<void()> onDismiss); }` — tela cheia por cima do Navigator;
    24 confetes caindo; título `fonts::title`, mensagem `fonts::medium`, assinatura `fonts::small`; toque fecha com fade.

- [ ] **Step 1: Teste**

```cpp
#include <unity.h>

#include "birthday/BirthdayCalendar.h"

void setUp()
{
}

void tearDown()
{
}

void test_first_boot_shows_even_without_a_clock()
{
    TEST_ASSERT_TRUE(BirthdayCalendar::shouldShow(true, std::nullopt, std::nullopt));
}

void test_after_first_boot_no_clock_means_no_show()
{
    TEST_ASSERT_FALSE(BirthdayCalendar::shouldShow(false, std::nullopt, 2026));
}

void test_birthday_next_year_shows_again()
{
    LocalTime birthday{8, 0, 22, 10, 2027, 5};

    TEST_ASSERT_TRUE(BirthdayCalendar::shouldShow(false, birthday, 2026));
}

void test_same_year_only_once()
{
    LocalTime birthday{20, 0, 22, 10, 2026, 4};

    TEST_ASSERT_FALSE(BirthdayCalendar::shouldShow(false, birthday, 2026));
}

void test_any_other_day_is_quiet()
{
    LocalTime dayAfter{8, 0, 23, 10, 2027, 6};

    TEST_ASSERT_FALSE(BirthdayCalendar::shouldShow(false, dayAfter, 2026));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_shows_even_without_a_clock);
    RUN_TEST(test_after_first_boot_no_clock_means_no_show);
    RUN_TEST(test_birthday_next_year_shows_again);
    RUN_TEST(test_same_year_only_once);
    RUN_TEST(test_any_other_day_is_quiet);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar; implementar**

```cpp
// birthday/BirthdayContent.h
#pragma once

// Edite antes de gravar a placa. Quebras de linha com \n; a tela tem ~40 caracteres por linha na fonte média.
namespace birthday {

constexpr int DAY = 22;
constexpr int MONTH = 10;
constexpr const char* TITLE = "Feliz aniversário, pai";
constexpr const char* MESSAGE = "Sua mensagem aqui, do jeito que você quiser escrever.";
constexpr const char* SIGNATURE = "— Leo";

}
```

```cpp
// birthday/BirthdayCalendar.h
#pragma once

#include <optional>

#include "support/LocalTime.h"

class BirthdayCalendar {
public:
    static bool shouldShow(bool neverShown, std::optional<LocalTime> now, std::optional<int> lastShownYear);
};
```

```cpp
// birthday/BirthdayCalendar.cpp
#include "birthday/BirthdayCalendar.h"

#include "birthday/BirthdayContent.h"

bool BirthdayCalendar::shouldShow(bool neverShown, std::optional<LocalTime> now, std::optional<int> lastShownYear)
{
    // O presente é aberto antes de qualquer Wi-Fi: na primeira vez a mensagem aparece sem precisar de relógio.
    if (neverShown) {
        return true;
    }

    if (!now || now->day != birthday::DAY || now->month != birthday::MONTH) {
        return false;
    }

    return !lastShownYear || *lastShownYear != now->year;
}
```

```cpp
// birthday/storage/BirthdayMemory.h
#pragma once

#include <Preferences.h>

#include <optional>

class BirthdayMemory {
public:
    void begin();
    bool neverShown() const;
    std::optional<int> lastShownYear() const;
    void markShown(std::optional<int> year);

private:
    Preferences preferences;
    bool shown = false;
    int year = 0;
};
```

```cpp
// birthday/storage/BirthdayMemory.cpp
#include "birthday/storage/BirthdayMemory.h"

namespace {

constexpr const char* NAMESPACE = "birthday";

}

void BirthdayMemory::begin()
{
    preferences.begin(NAMESPACE, false);
    shown = preferences.getBool("shown", false);
    year = preferences.getInt("year", 0);
}

bool BirthdayMemory::neverShown() const
{
    return !shown;
}

std::optional<int> BirthdayMemory::lastShownYear() const
{
    return year == 0 ? std::nullopt : std::optional<int>(year);
}

void BirthdayMemory::markShown(std::optional<int> shownYear)
{
    shown = true;
    preferences.putBool("shown", true);

    if (shownYear) {
        year = *shownYear;
        preferences.putInt("year", year);
    }
}
```

```cpp
// birthday/screen/BirthdayOverlay.h
#pragma once

#include <lvgl.h>

#include <functional>

class BirthdayOverlay {
public:
    void show(std::function<void()> onDismiss);

private:
    static void onTap(lv_event_t* event);
    void addConfetti(lv_obj_t* parent);

    lv_obj_t* layer = nullptr;
    std::function<void()> dismiss;
};
```

```cpp
// birthday/screen/BirthdayOverlay.cpp
#include "birthday/screen/BirthdayOverlay.h"

#include <Arduino.h>

#include "birthday/BirthdayContent.h"
#include "board/ScreenGeometry.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int CONFETTI_COUNT = 24;
constexpr uint32_t FALL_MIN_MILLIS = 3000;
constexpr uint32_t FALL_MAX_MILLIS = 5000;
constexpr uint32_t FADE_OUT_MILLIS = 400;
constexpr uint32_t COLORS[] = {0xD97757, 0xE8B04A, 0x8BB8E8, 0x9FD49A};

void fall(void* object, int32_t y)
{
    lv_obj_set_y(static_cast<lv_obj_t*>(object), y);
}

}

void BirthdayOverlay::show(std::function<void()> onDismiss)
{
    dismiss = std::move(onDismiss);

    layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(layer, theme::background(), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_COVER, 0);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(layer, onTap, LV_EVENT_CLICKED, this);

    addConfetti(layer);

    lv_obj_t* date = theme::label(layer, fonts::small, theme::textSecondary());
    lv_label_set_text_fmt(date, "%d de outubro", birthday::DAY);
    lv_obj_align(date, LV_ALIGN_CENTER, 0, -70);

    lv_obj_t* title = theme::label(layer, fonts::title, theme::text());
    lv_label_set_text(title, birthday::TITLE);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t* message = theme::label(layer, fonts::medium, theme::text());
    lv_label_set_text(message, birthday::MESSAGE);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(message, 320);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(message, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t* signature = theme::label(layer, fonts::small, theme::textSecondary());
    lv_label_set_text(signature, birthday::SIGNATURE);
    lv_obj_align(signature, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t* hint = theme::label(layer, fonts::small, theme::textSecondary());
    lv_label_set_text(hint, "toque para começar");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void BirthdayOverlay::addConfetti(lv_obj_t* parent)
{
    for (int piece = 0; piece < CONFETTI_COUNT; piece++) {
        lv_obj_t* confetti = lv_obj_create(parent);
        lv_obj_remove_style_all(confetti);
        int32_t size = 5 + random(4);
        lv_obj_set_size(confetti, size, size);
        lv_obj_set_style_bg_color(confetti, lv_color_hex(COLORS[piece % 4]), 0);
        lv_obj_set_style_bg_opa(confetti, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(confetti, piece % 3 == 0 ? LV_RADIUS_CIRCLE : 1, 0);
        lv_obj_set_x(confetti, random(SCREEN_WIDTH));

        lv_anim_t drop;
        lv_anim_init(&drop);
        lv_anim_set_var(&drop, confetti);
        lv_anim_set_values(&drop, -10, SCREEN_HEIGHT + 10);
        lv_anim_set_duration(&drop, FALL_MIN_MILLIS + random(FALL_MAX_MILLIS - FALL_MIN_MILLIS));
        lv_anim_set_delay(&drop, random(FALL_MAX_MILLIS));
        lv_anim_set_repeat_count(&drop, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&drop, fall);
        lv_anim_start(&drop);
    }
}

void BirthdayOverlay::onTap(lv_event_t* event)
{
    BirthdayOverlay* self = static_cast<BirthdayOverlay*>(lv_event_get_user_data(event));

    lv_obj_fade_out(self->layer, FADE_OUT_MILLIS, 0);
    lv_obj_delete_delayed(self->layer, FADE_OUT_MILLIS);
    self->layer = nullptr;

    if (self->dismiss) {
        self->dismiss();
    }
}
```

Run: `pio test -e native` → `41 succeeded`.

- [ ] **Step 3: `main.cpp`**

Globais `BirthdayMemory birthdayMemory; BirthdayOverlay birthdayOverlay;`. Em `setup()`, depois de
`navigator.begin()`:

```cpp
birthdayMemory.begin();
if (BirthdayCalendar::shouldShow(birthdayMemory.neverShown(), LocalTime::now(), birthdayMemory.lastShownYear())) {
    birthdayOverlay.show([]() {
        std::optional<LocalTime> now = LocalTime::now();
        birthdayMemory.markShown(now ? std::optional<int>(now->year) : std::nullopt);
    });
}
```

A tarefa de rede já sobe em paralelo: a mensagem nunca espera Wi-Fi.

- [ ] **Step 4: Na placa**

Run: `cd firmware && pio run -e board -t erase && pio run -e board -t upload --upload-port COM8`
Expected: mensagem com confete sem Wi-Fi; toque fecha com fade e a tela de configuração aparece; reiniciar → não
aparece de novo. Roteiro:

```markdown
## Aniversário

- [ ] Primeiro boot, sem Wi-Fi: mensagem aparece com confete; toque fecha.
- [ ] Reiniciar: não aparece de novo.
```

- [ ] **Step 5: Commit**

```bash
git add firmware docs/roteiro-de-teste.md
git commit -m "feat(birthday): mensagem de aniversário no primeiro boot e todo 22/10

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 10: Montagem final, guia de páginas e README

**Files:**
- Modify: `firmware/src/main.cpp`
- Create: `docs/como-criar-uma-pagina.md`
- Create: `README.md`
- Modify: `docs/roteiro-de-teste.md`

- [ ] **Step 1: `main.cpp` definitivo**

Ordem em `setup()`: serial → `lv_init` + tick → `display.begin()` → `theme::apply` → `touch.begin()` →
`backlight.begin()` → `photoStore.begin()` → `adminSecret.begin()` → Navigator com `SetupPage` (se não configurado)
ou `HomePage` (se configurado) → `navigator.begin()` → aniversário → tarefa de rede. Remova qualquer resto das
páginas de exemplo e do bring-up. O `loop()` fica: `navigator.tick(millis()); delay(min(lv_timer_handler(), 10));`
mais a checagem de `SetupStep::Done` → `ESP.restart()`.

- [ ] **Step 2: `docs/como-criar-uma-pagina.md`**

Conteúdo: (1) o que é uma `Page` e seus quatro métodos; (2) exemplo completo `src/hello/screen/HelloPage.h/.cpp` —
um label que mostra os segundos desde o boot em `onTick`; (3) registrar em `main.cpp` com
`navigator.add(std::make_unique<HelloPage>());`; (4) a regra "rede publica, UI lê" com um `Published<HelloState>`
preenchido na `networkTask` e lido no `onTick`; (5) onde fica lógica pura e como testá-la com `pio test -e native`
(exemplo de `test/hello/test_hello_counter/test_main.cpp`); (6) `theme::label`, `fonts::*`, cores do tema; (7) a
lista do que ainda não existe (seção 18 da especificação) como convite.

- [ ] **Step 3: `README.md`**

Seções: o que é (uma frase e um espaço para a foto), hardware, como compilar e gravar no Mac
(`brew install platformio`, `pio run -e board -t upload`), como configurar na primeira vez, como mandar fotos, como
rodar os testes, estrutura de pastas em uma tabela de uma linha por conceito, links para a especificação, o plano, o
roteiro e o guia de páginas, e "ideias para você" (seção 18 da especificação).

- [ ] **Step 4: Roteiro "Boot completo"**

```markdown
## Boot completo

- [ ] `pio run -t erase` → aniversário → portal → página → senha → reinicia → fotos → Início com relógio.
- [ ] Desligar e ligar: Início direto em menos de 5 s, fotos e relógio no lugar.
```

- [ ] **Step 5: Verificar e commitar**

Run: `cd firmware && pio test -e native && node --test test/web/ && pio run -e board`
Expected: tudo verde; nenhum warning novo do nosso código.

```bash
git add README.md docs firmware/src/main.cpp
git commit -m "docs: guia para criar páginas, README e roteiro de boot completo

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

## Self-review

- **Cobertura da especificação neste plano:** §2 hardware (T2), §3 tarefas e estado publicado (T3, T5), §4 stack
  (T1, T2, T4), §5 repositório e `Page` (T1, T3, T10), §6 telas Início/Aniversário/Configuração (T4, T5, T8, T9), §9
  configuração inicial (T5, T6), §10 fotos (T7, T8), §12 sem Wi-Fi/sem hora/flash cheia (T5, T7, T8), §13 senha e
  nome de arquivo (T6, T7), §14 testes (todas), §15 prioridade 0 e essencial. **Fora deste plano, de propósito:** §7
  Spotify e relay (plano 3), §8 Claude e alerta (plano 2), §11 OTA, modo noite, volta automática ao Início e toast
  (plano 4), pílula da música no Início (plano 3).
- **Tipos consistentes:** `Published<T>::readIfChanged(T&, uint32_t&)` em T3, T5; `PhotoName::parse/fromSequence/
  fileName/path` em T7, T8; `PhotoStore::list/open/nextName` em T7, T8; `LocalTime::now/hourMinute/longDate` em T5,
  T6, T8, T9; `SetupState.step/qrText/ip` em T5, T6, T10; `PageIndex::jumpTo` em T3 (o esboço antigo com `goHome`
  foi substituído).
- **Review Focus → testes:** 1 → `crop.test.mjs` (T7); 2 → `test_upload_check` + roteiro (T7); 3 →
  `test_birthday_calendar` (T9) e `test_setup_progress` (T5); 4 → `test_photo_name` (T7); 5 → `test_shuffle_order`
  caso vazio (T8) + roteiro do Início.
- **Correções feitas nesta revisão:** o teste de recorte com foto pouco mais alta que 3:2 (T7) esperava `y = 17`;
  pela regra escrita o valor é 0 — corrigido no texto da tarefa. O esboço de `PhotoView::load` com callbacks de
  arquivo foi substituído pela versão `openRAM`. O handler de upload do `PhotoRoutes` ganhou a assinatura real do
  ESPAsyncWebServer.
- **Ponto frágil declarado:** `WebServer::authorize` decodifica o Basic à mão porque o `authenticate()` da biblioteca
  só compara com senha fixa; o nome exato do helper de base64 do core (`base64::decode` em `<base64.h>` ou
  `mbedtls_base64_decode`) é confirmado ao compilar na T6 — anotado no próprio passo.
