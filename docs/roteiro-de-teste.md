# Roteiro de teste na placa

O que os testes do PC não alcançam. Rode a seção da área que você mexeu antes de gravar a versão que vai para a mesa.

## Tela e touch

- [ ] Imagem estável, sem piscar.
- [ ] Texto legível em paisagem, do jeito que a placa fica na mesa.
- [ ] Toque cai onde o dedo encostou (cantos e centro).
- [ ] Taxa de quadros com animação: ≥ 20 FPS.
- [ ] Com a USB ligada no computador e nenhum monitor serial aberto, a tela segue animando por alguns minutos.

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
- [ ] Página pede senha na primeira vez e autentica depois; sem senha, /api/photos devolve 401.

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

- [ ] `pio run -t erase` → aniversário → portal → página → senha → reinicia → fotos → Início com relógio.
- [ ] Desligar e ligar: Início direto em menos de 5 s, fotos e relógio no lugar.
- [ ] Placa configurada: roteador cai e volta → o aparelho não reinicia.
- [ ] Placa ligada atravessando a meia-noite de 21 para 22/10: a mensagem aparece em até um minuto.
- [ ] Dois envios de foto ao mesmo tempo: o segundo recebe "outro envio em andamento"; o primeiro grava inteiro.
- [ ] Foto nova aparece no carrossel sem reiniciar.
