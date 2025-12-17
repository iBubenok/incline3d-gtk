/**
 * @file file_utils.cpp
 * @brief Вспомогательные функции для работы с файлами
 */

#include "file_utils.hpp"
#include <fstream>
#include <stdexcept>

namespace incline::io {

void atomicWrite(const std::filesystem::path& path, const std::string& content) {
    auto dir = path.parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }

    auto tmp = path;
    tmp += ".tmp";

    {
        std::ofstream ofs(tmp, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Не удалось открыть временный файл для записи: " + tmp.string());
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        throw std::runtime_error("Не удалось атомарно сохранить файл: " + path.string());
    }
}

} // namespace incline::io
