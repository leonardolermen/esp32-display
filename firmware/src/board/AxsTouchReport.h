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
