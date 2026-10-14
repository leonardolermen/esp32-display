#pragma once

#include <string>

// Números do jeito que se lê no Brasil: ponto de milhar, vírgula decimal, "R$ " na frente, espaço antes do "%".
namespace PriceFormat {

std::string reais(double value);
std::string percent(double value);

}
