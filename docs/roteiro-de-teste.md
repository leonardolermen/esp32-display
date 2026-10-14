# Roteiro de teste na placa

O que os testes do PC não alcançam. Rode a seção da área que você mexeu antes de gravar a versão que vai para a mesa.

## Tela e touch

- [ ] Imagem estável, sem piscar.
- [ ] Texto legível em paisagem, do jeito que a placa fica na mesa.
- [ ] Toque cai onde o dedo encostou (cantos e centro).
- [ ] Taxa de quadros com animação: ≥ 20 FPS.

Resultado do bring-up (preencher): versões usadas ______, init `type1`/`type2` ______, rotação ______, FPS ______.

## Navegação

- [ ] Deslizar para a esquerda avança; para a direita volta; nas pontas não acontece nada.
- [ ] Pontinhos acompanham a página atual.

## Tema

- [ ] Acentos (ç, ã, é) aparecem nas fontes.

## Configuração inicial

- [ ] Sem Wi-Fi salvo: QR da rede `Claudinho`; o celular entra e abre o portal.
- [ ] Depois de conectar: QR de claudinho.local e o IP por extenso.
- [ ] `ping claudinho.local` responde.
- [ ] Página pede o PIN na primeira vez e autentica depois (`admin` + PIN); sem PIN, /api/photos devolve 401.
- [ ] PIN com letras ou com 3 dígitos é recusado na página e pela API (`4 a 6 dígitos`).

## Fotos

- [ ] Foto em pé vem com a cabeça inteira (recorte a um terço de cima).
- [ ] Panorâmica centrada.
- [ ] Desligar no meio do envio não deixa arquivo pela metade.
- [ ] JPEG > 200 KB via curl é recusado com mensagem clara.

## Início

- [ ] Fotos trocam com fade a cada ~12 s, sem repetir até dar a volta.
- [ ] Relógio e data corretos; sem internet, a faixa fica escondida.
- [ ] Sem fotos: convite para mandar fotos.

## Aniversário

- [ ] Primeiro boot, sem Wi-Fi: mensagem aparece com confete; toque fecha.
- [ ] Reiniciar: não aparece de novo.

## Boot completo

- [ ] `pio run -t erase` → aniversário → portal → página → PIN → reinicia → fotos → Início com relógio.
- [ ] Desligar e ligar: Início direto em menos de 5 s, fotos e relógio no lugar.
- [ ] Placa configurada: roteador cai e volta → o aparelho não reinicia.
- [ ] Placa ligada atravessando a meia-noite de 21 para 22/10: a mensagem aparece em até um minuto.
- [ ] Dois envios de foto ao mesmo tempo: o segundo recebe "outro envio em andamento"; o primeiro grava inteiro.
- [ ] Foto nova aparece no carrossel sem reiniciar.

## Cripto

- [ ] Em até 2 min depois do Wi-Fi, a página mostra BTC, ETH e dólar; valores batem com coingecko.com e com o Google.
- [ ] Variação positiva em verde, negativa em vermelho, com a sparkline na mesma cor.
- [ ] Roteador desligado: valores ficam e o rodapé vira "visto às HH:MM"; religado, volta a "atualizado" em até 2 min.
- [ ] Página aberta antes da primeira resposta: traços e "buscando…", sem travar.

## Spotify

Antes, uma vez: redirect URI `https://leonardolermen.github.io/esp32-display/callback/` no app do Spotify; Pages com
fonte "GitHub Actions" (o workflow `relay` publica sozinho a cada push na `main`). Para publicar antes do merge: em
Settings → Environments → `github-pages`, libere a branch em "Deployment branches"; depois Actions → `relay` → Run
workflow, escolhendo a branch. Sem o Pages, o plano B funciona: o login para numa página 404 do github.io, e o endereço
dela colado no card resolve.

- [ ] Card Spotify em claudinho.local → Conectar → login no celular → volta para a placa → "Spotify conectando".
- [ ] Página do Spotify desconectada mostra QR; escanear com o celular também conecta.
- [ ] Plano B: colar o link onde o login parou no campo "O login não voltou para cá?" conecta.
- [ ] Música aparece em até 3 s com capa, título, artista, aparelho e progresso andando.
- [ ] Tocar/pausar, próxima e anterior respondem na hora; volume pelo slider muda no aparelho.
- [ ] Arrastar o volume para os lados não troca de página.
- [ ] Trocar de álbum troca a capa e a cor de fundo com transição.
- [ ] Sem aparelho tocando: "Abra o Spotify em algum aparelho".
- [ ] Pílula no Início com a música; some ao pausar.
- [ ] Reiniciar a placa mantém a conexão; Desconectar na página volta para o QR.
