#include "S11Parser.h"
#include <fstream>
#include <cctype>
#include <charconv>

S11Parser::ParseResult S11Parser::parseFile(const std::string& filePath, Measurement& measurement) {
    auto result = parseFileExpected(filePath);
    if (std::holds_alternative<Measurement>(result)) {
        measurement = std::move(std::get<Measurement>(result));
        return ParseResult::Success;
    }
    return std::get<ParseResult>(result);
}

S11Parser::ParseExpected S11Parser::parseFileExpected(const std::filesystem::path& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return ParseResult::FileNotFound;
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return ParseResult::FileNotFound;
    }
    
    Measurement measurement;
    std::string line;
    bool headerFound = false;
    
    const auto fileSize = std::filesystem::file_size(filePath);

    const size_t estimatedLines = fileSize / 50;
    measurement.reserve(estimatedLines);
    
    while (std::getline(file, line)) {
        const auto trimmedLine = trim(line);
        
        if (trimmedLine.empty()) {
            continue;
        }
        
        if (trimmedLine[0] == '#') {
            if (isValidHeader(trimmedLine)) {
                headerFound = true;
            }
            continue;
        }
        
        if (trimmedLine[0] == '!') {
            continue;
        }
        
        if (auto point = parseDataLine(trimmedLine)) {
            measurement.addPoint(point->frequency, point->s11);
        }
    }
    
    if (measurement.empty()) {
        return ParseResult::EmptyFile;
    }
    
    if (!headerFound) {
        return ParseResult::InvalidFormat;
    }
    
    return measurement;
}

bool S11Parser::isValidHeader(std::string_view line) noexcept {
    //# Hz S RI R 50
    const auto tokens = split(line, ' ');
    
    if (tokens.size() < 6) {
        return false;
    }
    
    return (tokens[0] == "#" && 
            tokens[1] == "Hz" && 
            tokens[2] == "S" && 
            tokens[3] == "RI" && 
            tokens[4] == "R");
}

std::optional<FrequencyPoint> S11Parser::parseDataLine(std::string_view line) noexcept {
    const auto tokens = split(line, ' ');
    
    if (tokens.size() < 3) {
        return std::nullopt;
    }
    
    double frequency, real_part, imag_part;
    
    auto [ptr1, ec1] = std::from_chars(tokens[0].data(), tokens[0].data() + tokens[0].size(), frequency);
    if (ec1 != std::errc{}) {
        return std::nullopt;
    }
    
    auto [ptr2, ec2] = std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), real_part);
    if (ec2 != std::errc{}) {
        return std::nullopt;
    }
    
    auto [ptr3, ec3] = std::from_chars(tokens[2].data(), tokens[2].data() + tokens[2].size(), imag_part);
    if (ec3 != std::errc{}) {
        return std::nullopt;
    }
    
    return FrequencyPoint{frequency, std::complex<double>(real_part, imag_part)};
}

std::vector<std::string_view> S11Parser::split(std::string_view str, char delimiter) noexcept {
    std::vector<std::string_view> tokens;
    
    size_t start = 0;
    for (size_t i = 0; i <= str.size(); ++i) {
        if (i == str.size() || str[i] == delimiter) {
            if (i > start) {
                const auto token = str.substr(start, i - start);
                const auto trimmed = trim(token);
                if (!trimmed.empty()) {
                    tokens.push_back(trimmed);
                }
            }
            start = i + 1;
        }
    }
    
    return tokens;
}

constexpr std::string_view S11Parser::trim(std::string_view str) noexcept {
    constexpr auto whitespace = " \t\r\n";
    
    const auto start = str.find_first_not_of(whitespace);
    if (start == std::string_view::npos) {
        return {};
    }
    
    const auto end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}
