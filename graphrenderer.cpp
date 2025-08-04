#include "GraphRenderer.h"
#include <cmath>
#include <algorithm>
#include <execution>
#include <QFont>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>


QImage GraphRenderer::renderGraph(const Measurement& measurement, int width, int height) {
    
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::white);
    
    if (measurement.empty()) {
        return image;
    }
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    constexpr int margin = 60;
    const GraphBounds bounds = calculateBounds(measurement);

    if (measurement.size() > 500) {
        auto gridFuture = QtConcurrent::run([&]() {
            QImage gridImage(width, height, QImage::Format_ARGB32);
            gridImage.fill(Qt::transparent);
            QPainter gridPainter(&gridImage);
            gridPainter.setRenderHint(QPainter::Antialiasing);
            drawGrid(gridPainter, bounds, width, height, margin);
            return gridImage;
        });
        
        auto pixelPoints = calculatePixelPoints(std::span(measurement.data), bounds, width, height, margin);
        
        QImage gridImage = gridFuture.result();
        painter.drawImage(0, 0, gridImage);
        
        drawAxes(painter, bounds, width, height, margin);
        drawLabels(painter, bounds, width, height, margin);
        
        painter.setPen(QPen(Qt::blue, 2));
        
        if (!pixelPoints.empty()) {
            for (size_t i = 0; i < pixelPoints.size(); ++i) {
                const auto& pixelPoint = pixelPoints[i];
                painter.drawEllipse(pixelPoint.point, 2, 2);
                
                if (i > 0) {
                    painter.drawLine(pixelPoints[i-1].point, pixelPoint.point);
                }
            }
        }
    } else {
        drawGrid(painter, bounds, width, height, margin);
        drawAxes(painter, bounds, width, height, margin);
        drawLabels(painter, bounds, width, height, margin);
        
        painter.setPen(QPen(Qt::blue, 2));
        
        QPointF prevPoint;
        bool firstPoint = true;
        
        for (const auto& point : measurement.data) {
            const double logMag = calculateLogMag(point.s11);
            const QPointF pixelPoint = mapToPixel(point.frequency, logMag, bounds, width, height, margin);
            
            painter.drawEllipse(pixelPoint, 2, 2);
            
            if (!firstPoint) {
                painter.drawLine(prevPoint, pixelPoint);
            }
            
            prevPoint = pixelPoint;
            firstPoint = false;
        }
    }
    
    return image;
}


GraphRenderer::GraphBounds GraphRenderer::calculateBounds(const Measurement& measurement) {
    GraphBounds bounds{};
    
    if (measurement.empty()) {
        return bounds;
    }
    
    const auto& data = measurement.data;
    
    if (data.size() > 1000) {
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

QPointF GraphRenderer::mapToPixel(double freq, double mag, const GraphBounds& bounds, 
                                 int width, int height, int margin) noexcept {
    const double freqRange = bounds.maxFreq - bounds.minFreq;
    const double magRange = bounds.maxMag - bounds.minMag;
    const double plotWidth = width - 2 * margin;
    const double plotHeight = height - 2 * margin;
    
    const double x = margin + (freq - bounds.minFreq) / freqRange * plotWidth;
    const double y = height - margin - (mag - bounds.minMag) / magRange * plotHeight;
    
    return QPointF(std::clamp(x, 0.0, static_cast<double>(width)), 
                   std::clamp(y, 0.0, static_cast<double>(height)));
}

std::vector<GraphRenderer::PixelPoint> GraphRenderer::calculatePixelPoints(
    std::span<const FrequencyPoint> data, 
    const GraphBounds& bounds, 
    int width, int height, int margin) {
    
    std::vector<PixelPoint> pixelPoints(data.size());
    
    // Use parallel transform for large datasets
    if (data.size() > 500) {
        std::transform(
            std::execution::par_unseq,
            data.begin(), data.end(),
            pixelPoints.begin(),
            [&bounds, width, height, margin](const auto& point) {
                const double logMag = calculateLogMag(point.s11);
                return PixelPoint{
                    mapToPixel(point.frequency, logMag, bounds, width, height, margin),
                    logMag
                };
            }
        );
    } else {
        // Sequential for smaller datasets
        std::transform(
            data.begin(), data.end(),
            pixelPoints.begin(),
            [&bounds, width, height, margin](const auto& point) {
                const double logMag = calculateLogMag(point.s11);
                return PixelPoint{
                    mapToPixel(point.frequency, logMag, bounds, width, height, margin),
                    logMag
                };
            }
        );
    }
    
    return pixelPoints;
}

void GraphRenderer::drawAxes(QPainter& painter, const GraphBounds& bounds, 
                           int width, int height, int margin) {
    painter.setPen(QPen(Qt::black, 2));
    
    // X
    painter.drawLine(margin, height - margin, width - margin, height - margin);
    
    // Y
    painter.drawLine(margin, margin, margin, height - margin);
}

void GraphRenderer::drawGrid(QPainter& painter, const GraphBounds& bounds, 
                           int width, int height, int margin) {
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
        for (int i = 1; i < 10; ++i) {
        int x = margin + i * (width - 2 * margin) / 10;
        painter.drawLine(x, margin, x, height - margin);
    }
    
    for (int i = 1; i < 10; ++i) {
        int y = margin + i * (height - 2 * margin) / 10;
        painter.drawLine(margin, y, width - margin, y);
    }
}

void GraphRenderer::drawLabels(QPainter& painter, const GraphBounds& bounds, 
                             int width, int height, int margin) {
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    
    painter.drawText(width/2 - 30, height - 10, "Frequency (Hz)");
    
    painter.save();
    painter.translate(15, height/2);
    painter.rotate(-90);
    painter.drawText(-40, 0, "|S11| (dB)");
    painter.restore();
    
    const int numTicks = 5;
    
    for (int i = 0; i <= numTicks; ++i) {
        double freq = bounds.minFreq + i * (bounds.maxFreq - bounds.minFreq) / numTicks;
        int x = margin + i * (width - 2 * margin) / numTicks;
        
        QString freqStr;
        if (freq >= 1e9) {
            freqStr = QString::number(freq / 1e9, 'f', 1) + "G";
        } else if (freq >= 1e6) {
            freqStr = QString::number(freq / 1e6, 'f', 1) + "M";
        } else if (freq >= 1e3) {
            freqStr = QString::number(freq / 1e3, 'f', 1) + "k";
        } else {
            freqStr = QString::number(freq, 'f', 0);
        }
        
        painter.drawText(x - 20, height - margin + 20, freqStr);
    }
    
    for (int i = 0; i <= numTicks; ++i) {
        double mag = bounds.minMag + i * (bounds.maxMag - bounds.minMag) / numTicks;
        int y = height - margin - i * (height - 2 * margin) / numTicks;
        
        QString magStr = QString::number(mag, 'f', 1);
        painter.drawText(5, y + 5, magStr);
    }
}
