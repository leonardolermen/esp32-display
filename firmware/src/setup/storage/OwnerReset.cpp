#include "setup/storage/OwnerReset.h"

#include <Preferences.h>

namespace {

// Os namespaces de AdminSecret, SpotifyVault e BirthdayMemory. O Wi-Fi fica no namespace do próprio ESP-IDF e as
// fotos no LittleFS; nenhum dos dois é tocado.
constexpr const char* OWNER_NAMESPACES[] = {"admin", "spotify", "birthday"};

}

namespace OwnerReset {

void clearOwnerData()
{
    for (const char* name : OWNER_NAMESPACES) {
        Preferences preferences;
        preferences.begin(name, false);
        preferences.clear();
        preferences.end();
    }

    log_w("dados do dono apagados: PIN, Spotify e aniversário");
}

}
