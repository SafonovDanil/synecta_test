#pragma once

#include "Measurement.h"
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <span>

class GraphRenderer {
public:
    
    struct GraphBounds {
        double minFreq, maxFreq;
        double minMag, maxMag;
    };
    
    struct PixelPoint {
        QPointF point;
        double logMag;
    };
    
    static QImage renderGraph(const Measurement& measurement, int width, int height);
    static GraphBounds calculateBounds(const Measurement& measurement);
    static double calculateLogMag(const std::complex<double>& s11) noexcept;
    
private:
    static std::vector<PixelPoint> calculatePixelPoints(std::span<const FrequencyPoint> data, 
                                                       const GraphBounds& bounds, 
                                                       int width, int height, int margin);
    static QPointF mapToPixel(double freq, double mag, const GraphBounds& bounds, 
                             int width, int height, int margin) noexcept;
    static void drawAxes(QPainter& painter, const GraphBounds& bounds, 
                        int width, int height, int margin);
    static void drawGrid(QPainter& painter, const GraphBounds& bounds, 
                        int width, int height, int margin);
    static void drawLabels(QPainter& painter, const GraphBounds& bounds, 
                          int width, int height, int margin);
    
    template<typename T>
    static constexpr T clamp(T value, T min, T max) noexcept {
        static_assert(std::is_arithmetic_v<T>, "T must be arithmetic type");
        return std::max(min, std::min(value, max));
    }
};
