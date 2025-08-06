#pragma once

#include "Measurement.h"
#include <QImage>
#include <QPainter>

class GraphRenderer {
public:
    struct ZoomParams {
        double freqMin = 0.0;
        double freqMax = 0.0;
        double magMin = 0.0;
        double magMax = 0.0;
        bool isActive = false;
    };
    
    struct GraphBounds {
        double minFreq, maxFreq;
        double minMag, maxMag;
    };

    struct PixelPoint {
        QPointF point;
        double logMag;
    };
    
    static GraphBounds calculateBounds(const Measurement& measurement);
    static GraphBounds calculateBounds(const Measurement& measurement, const ZoomParams& zoom);
    static double calculateLogMag(const std::complex<double>& s11) noexcept;
};
