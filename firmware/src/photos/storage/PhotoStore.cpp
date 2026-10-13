#include "photos/storage/PhotoStore.h"

#include <LittleFS.h>

#include <algorithm>

namespace {

constexpr const char* DIRECTORY = "/photos";
constexpr const char* TEMP_PATH = "/photos/.upload.tmp";

}

void PhotoStore::begin()
{
    if (!LittleFS.begin(true)) {
        log_e("LittleFS did not mount");
        return;
    }
    LittleFS.mkdir(DIRECTORY);

    // Um envio interrompido (cabo puxado) não pode virar foto pela metade no carrossel.
    LittleFS.remove(TEMP_PATH);
}

std::vector<PhotoName> PhotoStore::list() const
{
    std::vector<PhotoName> names;

    File directory = LittleFS.open(DIRECTORY);
    for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
        std::optional<PhotoName> name = PhotoName::parse(entry.name());
        if (name) {
            names.push_back(*name);
        }
    }

    return names;
}

PhotoName PhotoStore::nextName() const
{
    uint16_t highest = 0;
    for (const PhotoName& name : list()) {
        highest = std::max<uint16_t>(highest, static_cast<uint16_t>(std::stoi(name.fileName().substr(1, 4))));
    }

    return PhotoName::fromSequence(highest + 1);
}

size_t PhotoStore::freeBytes() const
{
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

bool PhotoStore::beginUpload()
{
    upload = LittleFS.open(TEMP_PATH, FILE_WRITE);
    return static_cast<bool>(upload);
}

bool PhotoStore::writeChunk(const uint8_t* data, size_t length)
{
    return upload && upload.write(data, length) == length;
}

bool PhotoStore::finishUpload(const PhotoName& name)
{
    upload.close();
    return LittleFS.rename(TEMP_PATH, name.path().c_str());
}

void PhotoStore::abortUpload()
{
    upload.close();
    LittleFS.remove(TEMP_PATH);
}

bool PhotoStore::remove(const PhotoName& name)
{
    return LittleFS.remove(name.path().c_str());
}

File PhotoStore::open(const PhotoName& name) const
{
    return LittleFS.open(name.path().c_str(), FILE_READ);
}
