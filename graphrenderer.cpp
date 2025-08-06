#include "GraphRenderer.h"
#include <cmath>
#include <algorithm>
#include <execution>
#include <QFont>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

GraphRenderer::GraphBounds GraphRenderer::calculateBounds(const Measurement& measurement) {
    ZoomParams defaultZoom;
    return calculateBounds(measurement, defaultZoom);
}

// Пересчет границ
GraphRenderer::GraphBounds GraphRenderer::calculateBounds(const Measurement& measurement, const ZoomParams& zoom) {
    GraphBounds bounds{};
    
    if (measurement.empty()) {
        return bounds;
    }
    
    if (zoom.isActive) {
        bounds.minFreq = zoom.freqMin;
        bounds.maxFreq = zoom.freqMax;
        bounds.minMag = zoom.magMin;
        bounds.maxMag = zoom.magMax;
        
        if (bounds.minFreq >= bounds.maxFreq || bounds.minMag >= bounds.maxMag) {
            // invalid
        } else {
            return bounds;
        }
    }
    
    const auto& data = measurement.data;
    
    if (data.size() > 500) {
        auto freqRange = std::minmax_element(
            std::execution::par_unseq,
            data.begin(), data.end(),
            [](const auto& a, const auto& b) { return a.frequency < b.frequency; }
        );
        
        bounds.minFreq = freqRange.first->frequency;
        bounds.maxFreq = freqRange.second->frequency;
        
        std::vector<double> magnitudes(data.size());
        std::transform(
            std::execution::par_unseq,
            data.begin(), data.end(),
            magnitudes.begin(),
            [](const auto& point) { return calculateLogMag(point.s11); }
        );
        
        auto magRange = std::minmax_element(
            std::execution::par_unseq,
            magnitudes.begin(), magnitudes.end()
        );
        
        bounds.minMag = *magRange.first;
        bounds.maxMag = *magRange.second;
    } else {
        bounds.minFreq = bounds.maxFreq = data[0].frequency;
        const double firstMag = calculateLogMag(data[0].s11);
        bounds.minMag = bounds.maxMag = firstMag;
        
        for (const auto& point : data) {
            bounds.minFreq = std::min(bounds.minFreq, point.frequency);
            bounds.maxFreq = std::max(bounds.maxFreq, point.frequency);
            
            const double logMag = calculateLogMag(point.s11);
            bounds.minMag = std::min(bounds.minMag, logMag);
            bounds.maxMag = std::max(bounds.maxMag, logMag);
        }
    }
    
    constexpr double freqPadding = 0.05;
    constexpr double magPadding = 0.1;
    
    const double freqRange = bounds.maxFreq - bounds.minFreq;
    const double magRange = bounds.maxMag - bounds.minMag;
    
    bounds.minFreq -= freqRange * freqPadding;
    bounds.maxFreq += freqRange * freqPadding;
    bounds.minMag -= magRange * magPadding;
    bounds.maxMag += magRange * magPadding;
    
    return bounds;
}

double GraphRenderer::calculateLogMag(const std::complex<double>& s11) noexcept {
    const double magnitude = std::abs(s11);
    return 20.0 * std::log10(magnitude);
}

