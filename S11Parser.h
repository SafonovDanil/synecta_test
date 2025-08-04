#pragma once

#include "Measurement.h"
#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <variant>

class S11Parser {
public:
    enum class ParseResult {
        Success,
        FileNotFound,
        InvalidFormat,
        EmptyFile
    };
    
    using ParseExpected = std::variant<Measurement, ParseResult>;
    
    static ParseResult parseFile(const std::string& filePath, Measurement& measurement);
    static ParseExpected parseFileExpected(const std::filesystem::path& filePath);
    
private:
    static bool isValidHeader(std::string_view line) noexcept;
    static std::optional<FrequencyPoint> parseDataLine(std::string_view line) noexcept;
    static std::vector<std::string_view> split(std::string_view str, char delimiter) noexcept;
    static constexpr std::string_view trim(std::string_view str) noexcept;
    
};
