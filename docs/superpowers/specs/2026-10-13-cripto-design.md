# Cripto — design

- **Data:** 2026-10-13
- **Status:** aprovado em conversa
- **Complementa:** `2026-10-13-claudinho-design.md` (a seção 18 listava "cotações" fora da v1; esta spec traz para dentro)

## 1. Objetivo

Uma página do Claudinho com o preço do Bitcoin, do Ethereum e do dólar em reais, a variação do dia e uma linha
pequena de tendência, legível de longe. O pai acompanha cripto; a tela de mesa mostra sem ele precisar abrir nada.

## 2. Onde entra

Ordem do deslize: **Início → Cripto → Spotify → Claude**. A página vive em `firmware/src/crypto/` e segue a regra de
pastas do projeto: lógica pura na raiz do conceito, `device/` para rede, `screen/` para LVGL.

## 3. Tela

Paisagem 480×320, tema escuro. Três faixas empilhadas, uma por ativo, nesta ordem: Bitcoin, Ethereum, Dólar. Cada
faixa tem ~95 px de altura e um fundo `theme::surface()` com cantos arredondados (`theme::panel`).

| Região | Conteúdo | Fonte |
|---|---|---|
| Esquerda | símbolo (`BTC`, `ETH`, `USD`) em cima, nome (`Bitcoin`, `Ethereum`, `Dólar`) embaixo | `fonts::large` / `fonts::small` |
| Meio | preço em reais | `fonts::title` |
| Direita | variação do período com sinal, verde `#9FD49A` se ≥ 0, vermelho `#E24B4A` se < 0 | `fonts::medium` |
| Base da faixa | sparkline: linha de 1 px na cor da variação, sem eixos, sem pontos | `lv_chart` tipo linha |

Rodapé (fora das faixas, `fonts::small`, `theme::textSecondary()`): `atualizado HH:MM`. Quando a última tentativa
falhou, `visto às HH:MM`, com os últimos valores mantidos na tela. Antes da primeira resposta, os preços mostram `—`
e o rodapé `buscando…`.

Formato dos números (regra do português do Brasil):

| Valor | Texto |
|---|---|
| BTC e ETH: preço ≥ R$ 1.000 | sem centavos, ponto de milhar: `R$ 580.000` |
| Dólar (e qualquer preço < R$ 1.000) | duas casas, vírgula decimal: `R$ 5,43` |
| Variação | uma casa, vírgula, sinal sempre, espaço antes do `%`: `+2,3 %`, `-0,5 %` (hífen comum: as fontes geradas cobrem só Latin-1) |

Período da variação e da sparkline: **24 h** para BTC e ETH; **30 dias** para o dólar, que quase não mexe dentro do
dia. O rótulo do período aparece junto da variação em `fonts::small`: `24 h` ou `30 d`.

## 4. Dados

Atualização a cada **60 s** na tarefa de rede (núcleo 0), publicada à UI por `Published<MarketState>`. Timeout de 5 s
por chamada. Uma falha não derruba o resto: cada ativo é atualizado se a sua chamada respondeu.

| Ativo | Chamada | Campos usados |
|---|---|---|
| BTC e ETH | `GET https://api.coingecko.com/api/v3/coins/markets?vs_currency=brl&ids=bitcoin,ethereum&sparkline=true&price_change_percentage=24h` | `id`, `current_price`, `price_change_percentage_24h`, `sparkline_in_7d.price` (168 pontos horários; a tela usa os últimos 24) |
| Dólar (preço e variação) | `GET https://economia.awesomeapi.com.br/json/last/USD-BRL` | `USDBRL.bid`, `USDBRL.pctChange` (variação do dia; a tela mostra a de 30 d calculada da série abaixo) |
| Dólar (série) | `GET https://economia.awesomeapi.com.br/json/daily/USD-BRL/30` | `bid` de cada dia, do mais novo para o mais antigo |

A variação do dólar em 30 d é `(último − primeiro) / primeiro`, calculada na placa a partir da série.

A resposta do CoinGecko é grande (duas séries de 168 pontos). A leitura usa o filtro do ArduinoJson para carregar só
os campos da tabela e cabe na RAM interna; se ainda assim passar de 16 KB, o documento vai para a PSRAM.

Limites das APIs: CoinGecko gratuito aceita ~30 chamadas por minuto por IP; AwesomeAPI não publica limite. A 1 chamada
por minuto cada, sobra folga. `429` → espera o dobro (até 10 min) e volta ao normal na próxima resposta boa.

### HTTPS

As duas APIs só respondem em HTTPS. A conexão usa o pacote de certificados raiz do core do ESP32 (`setCACertBundle`
com o bundle embutido pelo pioarduino). Se o core desta versão não trouxer o bundle, cotação usa `setInsecure()` e
isso fica registrado em comentário no código: é dado só exibido, sem segredo nem comando; o pior caso é alguém na
rede mostrar um preço errado. **Essa exceção não vale para o Spotify**, que carrega tokens e exige verificação.

## 5. Código

```
crypto/
  Asset.h              enum class Asset { Bitcoin, Ethereum, Dollar } + símbolo, nome e período
  MarketState.h        struct Quote { double priceBrl; double changePercent; std::vector<float> series; bool known; }
                       struct MarketState { Quote quotes[3]; std::optional<LocalTime> updatedAt; bool lastFetchFailed; }
  PriceFormat.*        std::string reais(double); std::string percent(double)   — regras da seção 3
  MarketParser.*       std::optional<Quote> parseCoinGecko(json, const char* id); parseDollarLast(json);
                       parseDollarSeries(json)                                  — ArduinoJson, roda no PC
  Sparkline.*          std::vector<int32_t> toChart(series, maxPoints)          — reamostra para N pontos; lv_chart
                       recebe inteiros, então os valores viram centavos (×100) e o chart escala sozinho
  device/MarketFeed.*  faz as três chamadas com HTTPClient, monta MarketState, publica; cuida do 429
  screen/CryptoPage.*  Page: build cria três faixas e o rodapé; onTick lê readIfChanged e redesenha
```

`MarketFeed::poll()` é chamado da `networkTask` já existente, no mesmo `for (;;)`, com o próprio controle de intervalo.
Nada de tarefa nova.

## 6. Erros

| Situação | O que acontece |
|---|---|
| Sem Wi-Fi | `poll()` não chama nada; a tela mantém o último estado e o rodapé `visto às` |
| Uma API falha (timeout, 5xx, JSON inválido) | os outros ativos atualizam; `lastFetchFailed = true`; rodapé `visto às` |
| Campo faltando no JSON | o ativo fica `known = false` e mostra `—`; não derruba os outros |
| Série vazia | sparkline escondida para aquele ativo |
| `429` | recuo exponencial até 10 min |
| Relógio ainda sem NTP | rodapé sem horário: `atualizado` / `visto` sozinhos |

## 7. Testes

No PC (`pio test -e native`), espelhando `src/`:

- `test/crypto/test_price_format`: `580000.4 → "R$ 580.000"`, `5.4321 → "R$ 5,43"`, `999.99 → "R$ 999,99"`,
  `1000 → "R$ 1.000"`, `2.34 → "+2,3 %"`, `-0.5 → "−0,5 %"`, `0 → "+0,0 %"`.
- `test/crypto/test_market_parser`: JSON real das duas APIs (recortado) → preço, variação e série; `id` ausente;
  `current_price` nulo; série vazia; `daily` com um ponto só.
- `test/crypto/test_sparkline`: 168 pontos → 24 pontos (últimos); 5 pontos → 5; vazio → vazio; valores em centavos.

Na placa (`docs/roteiro-de-teste.md`, seção "Cripto"): valores batem com o site; verde e vermelho; sparkline
desenhada; roteador desligado → `visto às` e valores ficam; religado → volta a atualizar em até 2 min.

## 8. Fora desta spec

Mais ativos, escolha de moeda, alertas de preço, gráfico grande com toque. Cabem numa página nova ou numa evolução
desta, e ficam na lista de ideias do README.
