#pragma once

#include <lvgl.h>

#include <cstdint>

// Uma página é um módulo: build cria os widgets uma vez, onTick lê o estado publicado e muda só o que mudou.
class Page {
public:
    virtual ~Page() = default;

    virtual void build(lv_obj_t* parent) = 0;

    virtual void onShow()
    {
    }

    virtual void onHide()
    {
    }

    virtual void onTick(uint32_t nowMillis)
    {
    }
};
