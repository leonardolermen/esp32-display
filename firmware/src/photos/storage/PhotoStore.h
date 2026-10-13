#pragma once

#include <FS.h>

#include <vector>

#include "photos/PhotoName.h"

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

private:
    File upload;
};
