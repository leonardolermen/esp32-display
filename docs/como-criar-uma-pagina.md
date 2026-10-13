# Como criar uma página

O Claudinho é uma fila de páginas que você percorre deslizando o dedo. Cada página é uma classe que herda de `Page`,
vive na própria pasta e não sabe nada das outras. Este guia mostra uma página nova do zero, do label ao teste.

## 1. O que é uma `Page`

`firmware/src/navigation/Page.h` define quatro métodos:

| Método | Quando roda | Para quê |
|---|---|---|
| `build(lv_obj_t* parent)` | uma vez, no boot | criar os widgets dentro de `parent` (um container de 480×320) |
| `onShow()` | quando a página entra na tela | recarregar o que pode ter mudado enquanto ela estava escondida |
| `onHide()` | quando a página sai da tela | parar animações, soltar o que for caro |
| `onTick(uint32_t nowMillis)` | a cada volta do loop, só na página visível | ler estado novo e mudar só o que mudou |

Só `build` é obrigatório. O `Navigator` cuida do deslize, da animação e dos pontinhos embaixo.

## 2. Exemplo: segundos desde o boot

Crie a pasta do conceito e, dentro dela, a subpasta `screen/` (tudo que toca LVGL mora numa subpasta `screen/`):

`firmware/src/hello/screen/HelloPage.h`

```cpp
#pragma once

#include "navigation/Page.h"

class HelloPage : public Page {
public:
    void build(lv_obj_t* parent) override;
    void onTick(uint32_t nowMillis) override;

private:
    lv_obj_t* counter = nullptr;
    uint32_t shownSeconds = 0;
};
```

`firmware/src/hello/screen/HelloPage.cpp`

```cpp
#include "hello/screen/HelloPage.h"

#include "theme/Fonts.h"
#include "theme/Theme.h"

void HelloPage::build(lv_obj_t* parent)
{
    lv_obj_t* title = theme::label(parent, fonts::large, theme::textSecondary());
    lv_label_set_text(title, "Ligado há");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    counter = theme::label(parent, fonts::clock_, theme::text());
    lv_label_set_text(counter, "0");
    lv_obj_center(counter);
}

void HelloPage::onTick(uint32_t nowMillis)
{
    uint32_t seconds = nowMillis / 1000;
    if (seconds == shownSeconds) {
        return;
    }

    shownSeconds = seconds;
    lv_label_set_text_fmt(counter, "%lu", static_cast<unsigned long>(seconds));
}
```

O `if` no `onTick` importa: o loop roda dezenas de vezes por segundo e o LVGL redesenha tudo que você toca. Mude o
widget só quando o valor mudou.

## 3. Registrar no `main.cpp`

Em `setup()`, junto das outras páginas:

```cpp
navigator.add(std::make_unique<HelloPage>());
```

A ordem dos `add` é a ordem do deslize. A primeira é o Início.

## 4. Rede publica, UI lê

Regra do firmware: **só o loop do Arduino (núcleo 1) chama LVGL**. Wi-Fi, HTTP e qualquer coisa que espere a rede
rodam na `networkTask` (núcleo 0). Os dois lados conversam por `Published<T>`:

```cpp
// hello/HelloState.h — lógica pura, sem LVGL nem Arduino
struct HelloState {
    int visitors = 0;
};
```

Na `networkTask`, publique uma cópia sempre que souber algo novo:

```cpp
Published<HelloState> helloState;   // global em main.cpp, passada para a página no construtor

helloState.publish(HelloState{visitors});
```

Na página, leia no `onTick` e redesenhe só quando a versão mudou:

```cpp
void HelloPage::onTick(uint32_t)
{
    HelloState current;
    if (state.readIfChanged(current, seenVersion)) {
        lv_label_set_text_fmt(counter, "%d", current.visitors);
    }
}
```

`readIfChanged` devolve `false` enquanto ninguém publicou nada novo. Nenhum lado segura o outro por mais do que a
cópia de um struct.

## 5. Lógica pura e teste no PC

Tudo que não precisa de hardware fica fora de `device/`, `screen/`, `web/`, `storage/` e compila no PC. Se a sua
página tem uma regra (formatar, decidir, contar), ela vira uma classe pura com teste:

`firmware/src/hello/HelloCounter.h`

```cpp
#pragma once

#include <cstdint>
#include <string>

class HelloCounter {
public:
    static std::string describe(uint32_t seconds);
};
```

`firmware/test/hello/test_hello_counter/test_main.cpp`

```cpp
#include <unity.h>

#include "hello/HelloCounter.h"

void setUp()
{
}

void tearDown()
{
}

void test_under_a_minute_shows_seconds()
{
    TEST_ASSERT_EQUAL_STRING("45 s", HelloCounter::describe(45).c_str());
}

void test_over_a_minute_shows_minutes()
{
    TEST_ASSERT_EQUAL_STRING("2 min", HelloCounter::describe(130).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_under_a_minute_shows_seconds);
    RUN_TEST(test_over_a_minute_shows_minutes);
    return UNITY_END();
}
```

Rode de dentro de `firmware/`:

```bash
pio test -e native
```

O teste espelha o arquivo de `src/`: `src/hello/HelloCounter.cpp` → `test/hello/test_hello_counter/test_main.cpp`.
Escreva o teste antes, veja falhar, aí implemente.

## 6. Tema e fontes

Use sempre `theme` e `fonts` em vez de cores e fontes soltas, para a página parecer parte do aparelho:

| Nome | Uso |
|---|---|
| `theme::background()` | fundo da tela (`#1F1E1C`) |
| `theme::surface()` | cartões e painéis (`#34322F`) |
| `theme::text()` | texto principal (`#F5F1EA`) |
| `theme::textSecondary()` | legendas e dicas (`#A8A29A`) |
| `theme::accent()` | o laranja do Claude (`#D97757`) |
| `theme::amber()` | destaque quente (`#E8B04A`) |
| `fonts::small` / `medium` / `large` / `title` | Inter 12 / 14 / 18 / 24, com acentos |
| `fonts::clock_` | Inter 64, só dígitos e `:` (para números grandes) |

`theme::label(parent, font, color)` cria um label já estilizado; `theme::panel(parent)` cria um cartão com cantos
arredondados.

## 7. O que ainda não existe

Ideias que ficaram de fora da primeira versão e cabem numa página nova: clima, GitHub (contribuições, PRs, CI),
cotações, recados e fotos por Telegram, placar de futebol, pomodoro, microSD, som, simulador no PC e uma API de
"página genérica" para alimentar a tela de qualquer linguagem (Go incluso). Cada uma é uma pasta nova, uma `Page` e
um `Published<T>`.
