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
