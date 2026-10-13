#include "navigation/PageIndex.h"

#include <algorithm>

PageIndex::PageIndex(size_t count)
    : count(count)
{
}

size_t PageIndex::current() const
{
    return index;
}

bool PageIndex::next()
{
    if (index + 1 >= count) {
        return false;
    }

    index++;
    return true;
}

bool PageIndex::previous()
{
    if (index == 0) {
        return false;
    }

    index--;
    return true;
}

void PageIndex::jumpTo(size_t target)
{
    index = std::min(target, count - 1);
}
