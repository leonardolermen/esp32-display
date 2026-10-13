#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Só o servidor dá nome às fotos (p0001.jpg…): nome vindo do cliente nunca vira caminho de arquivo.
class PhotoName {
public:
    static std::optional<PhotoName> parse(const std::string& fileName);
    static PhotoName fromSequence(uint16_t sequence);

    const std::string& fileName() const;
    std::string path() const;

private:
    explicit PhotoName(std::string fileName);

    std::string name;
};
