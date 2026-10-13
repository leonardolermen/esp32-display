#include "photos/UploadCheck.h"

std::optional<std::string> UploadCheck::refuse(size_t declaredBytes, size_t freeBytes, size_t photoCount)
{
    if (declaredBytes > MAX_PHOTO_BYTES) {
        return "foto maior que 200 KB";
    }

    if (photoCount >= MAX_PHOTOS) {
        return "limite de 200 fotos";
    }

    // A reserva evita que uma foto deixe o LittleFS sem espaço para o próprio metadado e para o histórico do Claude.
    if (freeBytes < declaredBytes + MIN_FREE_BYTES) {
        return "memória cheia";
    }

    return std::nullopt;
}
