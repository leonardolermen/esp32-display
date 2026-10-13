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
- [ ] Página pede senha na primeira vez e autentica depois; sem senha, /api/photos devolve 401.
