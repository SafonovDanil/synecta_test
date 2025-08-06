#pragma once

#include <chrono>
#include <string>
#include <iostream>


namespace perf {

// RAII таймер
class ScopedTimer {
public:
    explicit ScopedTimer(std::string name) 
        : m_name(std::move(name))
        , m_start(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedTimer() {
        const auto end = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        std::cout << m_name << " took: " << duration.count() << " ms\n";
    }

private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

// Функция для замера времени выполнения
template<typename F>
auto measure(F&& func, const std::string& name = "Function") {
    static_assert(std::is_invocable_v<F>, "F must be invocable");
    ScopedTimer timer(name);
    if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
        func();
    } else {
        return func();
    }
}

} // namespace perf

#define PERF_MEASURE(name) perf::ScopedTimer _timer(name)
