#pragma once

#include "board/ScreenGeometry.h"

// O AXS15231B não gira a imagem por hardware: a UI desenha em paisagem e cada quadro é girado aqui para o painel
// retrato. O touch volta nas coordenadas do painel e passa pela mesma rotação ao contrário, então tela e toque
// nunca discordam. Rotation90 é a orientação do exemplo F1ATB (setRotation(1)); Rotation270 é a placa de cabeça
// para baixo.
class ScreenRotation {
public:
    virtual ~ScreenRotation() = default;

    // Gira as linhas [firstRow, firstRow + rowCount) do painel a partir do quadro paisagem inteiro. O quadro pode
    // ir ao painel em faixas, cada uma num buffer pequeno, sem nunca existir girado por inteiro.
    virtual void rotateRows(
        const uint16_t* landscape, uint16_t firstRow, uint16_t rowCount, uint16_t* portraitRows) const = 0;
    virtual ScreenPoint toScreen(PanelPoint touched) const = 0;
};

class Rotation90 : public ScreenRotation {
public:
    void rotateRows(
        const uint16_t* landscape, uint16_t firstRow, uint16_t rowCount, uint16_t* portraitRows) const override;
    ScreenPoint toScreen(PanelPoint touched) const override;
};

class Rotation270 : public ScreenRotation {
public:
    void rotateRows(
        const uint16_t* landscape, uint16_t firstRow, uint16_t rowCount, uint16_t* portraitRows) const override;
    ScreenPoint toScreen(PanelPoint touched) const override;
};
