#pragma once

#include <JPEGDEC.h>
#include <lvgl.h>

#include "photos/storage/PhotoStore.h"

class PhotoView {
public:
    explicit PhotoView(PhotoStore& store);

    bool load(const PhotoName& name, lv_image_dsc_t& into);

private:
    static int copyBlock(JPEGDRAW* block);

    PhotoStore& store;
    JPEGDEC decoder;
};
