#pragma once

// Prepara o aparelho para um novo dono sem perder o Wi-Fi nem as fotos: apaga o PIN, o login do Spotify e a marca de
// "mensagem de aniversário já mostrada". Só roda num firmware gravado com -D CLAUDINHO_OWNER_RESET (veja o roteiro).
namespace OwnerReset {

void clearOwnerData();

}
