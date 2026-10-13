#pragma once

#include <cstddef>

class PageIndex {
public:
    explicit PageIndex(size_t count);

    size_t current() const;
    bool next();
    bool previous();
    void jumpTo(size_t target);

private:
    size_t count;
    size_t index = 0;
};
