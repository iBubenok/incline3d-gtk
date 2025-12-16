/**
 * @file format_registry.cpp
 * @brief Реализация реестра форматов
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "format_registry.hpp"
#include <algorithm>
#include <cctype>

namespace incline::io {

std::string_view getFormatName(FileFormat format) noexcept {
    switch (format) {
        case FileFormat::Project: return "Проект Incline3D";
        case FileFormat::CSV: return "CSV (текст с разделителями)";
        case FileFormat::LAS: return "LAS 2.0";
        case FileFormat::Unknown:
        default: return "Неизвестный формат";
    }
}

std::vector<std::string> getFormatExtensions(FileFormat format) noexcept {
    switch (format) {
        case FileFormat::Project:
            return {".inclproj"};
        case FileFormat::CSV:
            return {".csv", ".txt"};
        case FileFormat::LAS:
            return {".las"};
        case FileFormat::Unknown:
        default:
            return {};
    }
}

DetectionInfo detectFormat(const std::filesystem::path& path) {
    DetectionInfo info;

    if (!std::filesystem::exists(path)) {
        info.result = FormatDetectionResult::Unknown;
        info.error_message = "Файл не найден";
        return info;
    }

    // Получаем расширение
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Проверяем по расширению
    if (ext == ".inclproj") {
        if (isProjectFile(path)) {
            info.result = FormatDetectionResult::Detected;
            info.format = FileFormat::Project;
            info.confidence = 1.0;
            return info;
        }
    }

    if (ext == ".las") {
        if (canReadLas(path)) {
            info.result = FormatDetectionResult::Detected;
            info.format = FileFormat::LAS;
            info.confidence = 0.95;
            return info;
        }
    }

    if (ext == ".csv" || ext == ".txt") {
        if (canReadCsv(path)) {
            auto detection = detectCsvFormat(path);
            info.result = FormatDetectionResult::Detected;
            info.format = FileFormat::CSV;
            info.confidence = detection.confidence;
            return info;
        }
    }

    // Пробуем определить по содержимому
    if (canReadLas(path)) {
        info.result = FormatDetectionResult::Detected;
        info.format = FileFormat::LAS;
        info.confidence = 0.8;
        return info;
    }

    if (canReadCsv(path)) {
        info.result = FormatDetectionResult::Detected;
        info.format = FileFormat::CSV;
        info.confidence = 0.5;
        return info;
    }

    info.result = FormatDetectionResult::Unknown;
    info.error_message = "Не удалось определить формат файла";
    return info;
}

ImportResult importMeasurements(const std::filesystem::path& path) {
    ImportResult result;

    auto detection = detectFormat(path);
    result.detected_format = detection.format;

    if (detection.result == FormatDetectionResult::Unknown) {
        result.success = false;
        result.error_message = detection.error_message;
        return result;
    }

    return importMeasurements(path, detection.format);
}

ImportResult importMeasurements(
    const std::filesystem::path& path,
    FileFormat format
) {
    ImportResult result;
    result.detected_format = format;

    try {
        switch (format) {
            case FileFormat::CSV: {
                result.data = readCsvMeasurements(path);
                result.success = true;
                break;
            }

            case FileFormat::LAS: {
                result.data = readLasMeasurements(path);
                result.success = true;
                break;
            }

            case FileFormat::Project: {
                auto project = loadProject(path);
                if (!project.wells.empty()) {
                    result.data = project.wells.front().source_data;
                    result.success = true;
                } else {
                    result.success = false;
                    result.error_message = "Проект не содержит скважин";
                }
                break;
            }

            case FileFormat::Unknown:
            default:
                result.success = false;
                result.error_message = "Неподдерживаемый формат файла";
                break;
        }
    } catch (const CsvReadError& e) {
        result.success = false;
        result.error_message = e.what();
        if (e.line() > 0) {
            result.error_message += " (строка " + std::to_string(e.line()) + ")";
        }
    } catch (const LasReadError& e) {
        result.success = false;
        result.error_message = e.what();
        if (e.line() > 0) {
            result.error_message += " (строка " + std::to_string(e.line()) + ")";
        }
    } catch (const ProjectError& e) {
        result.success = false;
        result.error_message = e.what();
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Ошибка чтения: " + std::string(e.what());
    }

    return result;
}

std::string getImportFileFilter() {
    // Формат для GtkFileFilter
    return "Все поддерживаемые (*.csv, *.las, *.txt)|*.csv;*.las;*.txt|"
           "CSV файлы (*.csv, *.txt)|*.csv;*.txt|"
           "LAS файлы (*.las)|*.las|"
           "Все файлы (*.*)|*";
}

std::string getExportFileFilter() {
    return "CSV файлы (*.csv)|*.csv|"
           "LAS файлы (*.las)|*.las|"
           "Все файлы (*.*)|*";
}

std::string getProjectFileFilter() {
    return "Проект Incline3D (*.inclproj)|*.inclproj|"
           "Все файлы (*.*)|*";
}

} // namespace incline::io
