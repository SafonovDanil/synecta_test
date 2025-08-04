#pragma once

#include <vector>
#include <complex>
#include <span>
#include <ranges>
#include <concepts>

struct FrequencyPoint {
    double frequency;
    std::complex<double> s11;
    
    constexpr FrequencyPoint() noexcept : frequency(0.0), s11(0.0, 0.0) {}
    
    constexpr FrequencyPoint(double freq, std::complex<double> s11_val) noexcept
        : frequency(freq), s11(s11_val) {}
};

class Measurement {
public:
    std::vector<FrequencyPoint> data;
    
    // Template function with static_assert for type safety
    template<typename T>
    void addPoint(T frequency, std::complex<T> s11) {
        static_assert(std::is_floating_point_v<T>, "T must be floating point type");
        data.emplace_back(static_cast<double>(frequency), 
                         std::complex<double>(s11.real(), s11.imag()));
    }
    
    void clear() noexcept {
        data.clear();
    }
    
    [[nodiscard]] constexpr size_t size() const noexcept {
        return data.size();
    }
    
    [[nodiscard]] constexpr bool empty() const noexcept {
        return data.empty();
    }
    
    [[nodiscard]] const FrequencyPoint& operator[](size_t index) const {
        return data[index];
    }
    
    [[nodiscard]] auto begin() const noexcept { return data.begin(); }
    [[nodiscard]] auto end() const noexcept { return data.end(); }
    [[nodiscard]] auto begin() noexcept { return data.begin(); }
    [[nodiscard]] auto end() noexcept { return data.end(); }
    
    void reserve(size_t capacity) {
        data.reserve(capacity);
    }
    
    [[nodiscard]] std::span<const FrequencyPoint> span() const noexcept {
        return std::span(data);
    }
    
    template<typename Predicate>
    [[nodiscard]] auto filter(Predicate&& pred) const {
        return data | std::views::filter(std::forward<Predicate>(pred));
    }
    
    template<typename Transform>
    [[nodiscard]] auto transform(Transform&& trans) const {
        return data | std::views::transform(std::forward<Transform>(trans));
    }
};
