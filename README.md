# Claudinho

Um aparelho de mesa que mostra fotos da família com relógio, feito para dar de presente e para ser aberto, entendido
e estendido por quem recebe.

_(foto do aparelho na mesa)_

## Hardware

- Placa **Guition JC3248W535C**: ESP32-S3 N16R8 (16 MB flash, 8 MB PSRAM), painel 320×480 AXS15231B em QSPI,
  touch capacitivo por I2C.
- A tela é usada em paisagem (480×320); a rotação é feita por software a cada quadro.

## Compilar e gravar (Mac)

```bash
brew install platformio
```

De dentro de `firmware/`, com a placa no USB:

```bash
pio run -e board -t upload
```

Para ler o que a placa imprime:

```bash
pio device monitor
```

**Nunca rode `pio run -t uploadfs`**: ele regrava a partição de arquivos inteira e apaga as fotos. A página web vai
embutida no firmware, então não precisa.

## Primeira vez

1. Ligue. A mensagem de aniversário aparece; toque para fechar.
2. A tela mostra um QR. Escaneie com o celular: ele entra na rede `Claudinho` e abre o portal para escolher o Wi-Fi
   de casa.
3. Conectou: a tela mostra o QR de `http://claudinho.local` e o IP. Abra no celular ou no computador e crie a senha
   de administrador (8 a 64 caracteres). O aparelho reinicia já no Início.

## Mandar fotos

Abra `http://claudinho.local`, entre com usuário `admin` e a senha, e use **Enviar fotos**. O navegador recorta cada
foto para 480×320 antes de enviar (em fotos em pé o recorte fica no terço de cima, onde estão os rostos). Dá para
apagar pela mesma página. Limites: 200 KB por foto, 200 fotos.

## Testes

De dentro de `firmware/`:

```bash
pio test -e native
```

```bash
node --test "test/web/*.test.mjs"
```

O que só se vê na placa está em [docs/roteiro-de-teste.md](docs/roteiro-de-teste.md).

## Estrutura

| Pasta | O que é |
|---|---|
| `firmware/src/board/` | tela, touch, backlight e a rotação de quadros |
| `firmware/src/navigation/` | `Page` e o `Navigator` (deslize entre páginas) |
| `firmware/src/theme/` | cores e fontes Inter |
| `firmware/src/setup/` | Wi-Fi, portal cativo, senha de administrador, servidor HTTP e tela de configuração |
| `firmware/src/photos/` | fotos: envio pelo navegador, armazenamento, carrossel do Início |
| `firmware/src/birthday/` | a mensagem de aniversário |
| `firmware/src/support/` | o que serve a todos: `Published<T>`, hora local, relógio NTP |
| `firmware/web/` | a página servida em `claudinho.local` |
| `firmware/test/` | testes do PC, espelhando `src/` |
| `docs/` | especificação, plano, roteiro de teste e guia de páginas |

Dentro de cada conceito, `device/`, `screen/`, `web/` e `storage/` guardam o que depende de hardware ou framework; o
resto é C++17 puro e roda no PC.

## Documentos

- [Especificação](docs/superpowers/specs/2026-10-13-claudinho-design.md)
- [Plano 1: fundação, fotos e aniversário](docs/superpowers/plans/2026-10-13-plano-1-fundacao-fotos-aniversario.md)
- [Roteiro de teste na placa](docs/roteiro-de-teste.md)
- [Como criar uma página](docs/como-criar-uma-pagina.md)

## Ideias para você

Clima, GitHub (contribuições, PRs, CI), cotações, recados e fotos por Telegram, placar de futebol, pomodoro,
microSD, som, simulador no PC, e uma API de "página genérica" para alimentar a tela de qualquer linguagem (Go
incluso). O [guia de páginas](docs/como-criar-uma-pagina.md) mostra por onde começar.
