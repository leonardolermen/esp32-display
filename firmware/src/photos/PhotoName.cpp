#include "photos/PhotoName.h"

#include <cctype>
#include <cstdio>

namespace {

constexpr const char* DIRECTORY = "/photos/";
constexpr size_t LENGTH = 9;

bool hasCanonicalForm(const std::string& fileName)
{
    if (fileName.size() != LENGTH || fileName[0] != 'p' || fileName.compare(5, 4, ".jpg") != 0) {
        return false;
    }

    for (size_t position = 1; position < 5; position++) {
        if (!std::isdigit(static_cast<unsigned char>(fileName[position]))) {
            return false;
        }
    }

    return true;
}

}

std::optional<PhotoName> PhotoName::parse(const std::string& fileName)
{
    if (!hasCanonicalForm(fileName)) {
        return std::nullopt;
    }

    return PhotoName(fileName);
}

PhotoName PhotoName::fromSequence(uint16_t sequence)
{
    char buffer[LENGTH + 1];
    snprintf(buffer, sizeof(buffer), "p%04u.jpg", static_cast<unsigned>(sequence % 10000));

    return PhotoName(buffer);
}

const std::string& PhotoName::fileName() const
{
    return name;
}

std::string PhotoName::path() const
{
    return DIRECTORY + name;
}

PhotoName::PhotoName(std::string fileName)
    : name(std::move(fileName))
{
}
