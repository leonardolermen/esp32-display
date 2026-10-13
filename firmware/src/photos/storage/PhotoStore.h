#pragma once

#include <FS.h>

#include <vector>

#include "photos/PhotoName.h"
#include "support/Published.h"

class PhotoStore {
public:
    void begin();
    std::vector<PhotoName> list() const;
    PhotoName nextName() const;
    size_t freeBytes() const;

    bool beginUpload();
    bool writeChunk(const uint8_t* data, size_t length);
    bool finishUpload(const PhotoName& name);
    void abortUpload();

    bool remove(const PhotoName& name);
    File open(const PhotoName& name) const;

    // Muda a cada foto gravada ou apagada; a UI relê a lista quando vê um número novo.
    const Published<uint32_t>& albumVersion() const;

private:
    void bumpAlbum();

    File upload;
    Published<uint32_t> album;
    uint32_t albumChanges = 0;
};
