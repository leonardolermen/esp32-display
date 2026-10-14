#pragma once

#include <JPEGDEC.h>

#include <memory>
#include <optional>
#include <string>

#include "spotify/CoverArt.h"

// Baixa a capa só quando o álbum muda, decodifica e reduz para 200×200 uma vez.
class CoverFetcher {
public:
    std::optional<CoverArt> fetch(const std::string& url);

private:
    static int copyBlock(JPEGDRAW* block);

    std::unique_ptr<JPEGDEC> decoder;
};
