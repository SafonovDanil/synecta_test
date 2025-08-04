#include "GraphWidget.h"
#include <QFont>
#include <QPen>
#include <QPainterPath>
#include <algorithm>
#include <mutex>
#include <cmath>

GraphWidget::GraphWidget(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(ItemHasContents, true);
    setAntialiasing(true);
}

void GraphWidget::paint(QPainter *painter) {

    const int width = static_cast<int>(this->width());
    const int height = static_cast<int>(this->height());
    
    if (width <= 0 || height <= 0) return;
    
    painter->fillRect(0, 0, width, height, Qt::white);
    
    if (m_isLoading.load(std::memory_order_relaxed)) {
        drawLoadingOverlay(painter);
        return;
    }
    
    std::shared_lock lock(m_dataMutex);
    
    if (m_measurement.empty()) {
        lock.unlock();
        drawEmptyState(painter);
        return;
    }
    
    constexpr int margin = 60;
    const int plotWidth = width - 2 * margin;
    const int plotHeight = height - 2 * margin;
    
    if (plotWidth <= 0 || plotHeight <= 0) return;
    
    const auto bounds = GraphRenderer::calculateBounds(m_measurement);
    const double freqRange = bounds.maxFreq - bounds.minFreq;
    const double magRange = bounds.maxMag - bounds.minMag;
    
    if (freqRange <= 0 || magRange <= 0) return;
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    const QPen gridPen(Qt::lightGray, 1, Qt::DotLine);
    painter->setPen(gridPen);
    
    for (int i = 1; i < 10; ++i) {
        const int x = margin + i * plotWidth / 10;
        painter->drawLine(x, margin, x, height - margin);
    }
    
    for (int i = 1; i < 10; ++i) {
        const int y = margin + i * plotHeight / 10;
        painter->drawLine(margin, y, width - margin, y);
    }
    
    const QPen axisPen(Qt::black, 2);
    painter->setPen(axisPen);
    painter->drawLine(margin, height - margin, width - margin, height - margin); // X-axis
    painter->drawLine(margin, margin, margin, height - margin); // Y-axis
    
    painter->setPen(Qt::black);
    QFont font = painter->font();
    font.setPointSize(10);
    painter->setFont(font);
    
    painter->drawText(width/2 - 30, height - 10, "Frequency (Hz)");
    
    painter->save();
    painter->translate(15, height/2);
    painter->rotate(-90);
    painter->drawText(-40, 0, "|S11| (dB)");
    painter->restore();
    
    constexpr int numTicks = 5;
    const double freqStep = freqRange / numTicks;
    const double magStep = magRange / numTicks;
    
    for (int i = 0; i <= numTicks; ++i) {
        const double freq = bounds.minFreq + i * freqStep;
        const int x = margin + i * plotWidth / numTicks;
        
        QString freqStr = formatFrequency(freq);
        painter->drawText(x - 20, height - margin + 20, freqStr);
    }
    
    for (int i = 0; i <= numTicks; ++i) {
        const double mag = bounds.minMag + i * magStep;
        const int y = height - margin - i * plotHeight / numTicks;
        
        const QString magStr = QString::number(mag, 'f', 1);
        painter->drawText(5, y + 5, magStr);
    }
    
    drawDataPoints(painter, bounds, width, height, margin);
    
    lock.unlock();
}


void GraphWidget::updateMeasurement(const Measurement& measurement) {
    {
        std::unique_lock lock(m_dataMutex);
        m_measurement = measurement;
    }
    
    setHasData(!measurement.empty());
    update();
}


void GraphWidget::setIsLoading(bool loading) {
    const bool oldValue = m_isLoading.exchange(loading);
    if (oldValue != loading) {
        emit isLoadingChanged();
        update();
    }
}


void GraphWidget::setHasData(bool hasData) {
    const bool oldValue = m_hasData.exchange(hasData);
    if (oldValue != hasData) {
        emit hasDataChanged();
    }
}

void GraphWidget::drawLoadingOverlay(QPainter *painter) {
    // Semi-transparent overlay
    painter->fillRect(0, 0, width(), height(), QColor(255, 255, 255, 200));
    
    // Loading text
    painter->setPen(Qt::black);
    QFont font = painter->font();
    font.setPointSize(14);
    painter->setFont(font);
    
    const QRectF textRect(0, 0, width(), height());

}

void GraphWidget::drawEmptyState(QPainter *painter) {
    painter->setPen(QColor(102, 102, 102)); // #666
    QFont font = painter->font();
    font.setPointSize(14);
    painter->setFont(font);
    
    const QRectF textRect(0, 0, width(), height());

}


QString GraphWidget::formatFrequency(double freq) const {
    if (freq >= 1e9) {
        return QString::number(freq / 1e9, 'f', 1) + "G";
    } else if (freq >= 1e6) {
        return QString::number(freq / 1e6, 'f', 1) + "M";
    } else if (freq >= 1e3) {
        return QString::number(freq / 1e3, 'f', 1) + "k";
    } else {
        return QString::number(freq, 'f', 0);
    }
}

void GraphWidget::drawDataPoints(QPainter *painter, const GraphRenderer::GraphBounds& bounds, 
                                int width, int height, int margin) {
    const QPen dataPen(Qt::blue, 2);
    painter->setPen(dataPen);
    
    const double freqRange = bounds.maxFreq - bounds.minFreq;
    const double magRange = bounds.maxMag - bounds.minMag;
    const double plotWidth = width - 2 * margin;
    const double plotHeight = height - 2 * margin;
    
    const double invFreqRange = 1.0 / freqRange;
    const double invMagRange = 1.0 / magRange;
    
    QPainterPath path;
    bool firstPoint = true;
    
    const size_t dataSize = m_measurement.data.size();
    if (dataSize > 1000) {
        const size_t step = std::max(size_t(1), dataSize / 2000);
        
        for (size_t i = 0; i < dataSize; i += step) {
            const auto& point = m_measurement.data[i];
            const double logMag = GraphRenderer::calculateLogMag(point.s11);
            
            const double x = margin + (point.frequency - bounds.minFreq) * invFreqRange * plotWidth;
            const double y = height - margin - (logMag - bounds.minMag) * invMagRange * plotHeight;
            
            const QPointF pixelPoint(
                std::clamp(x, static_cast<double>(margin), static_cast<double>(width - margin)),
                std::clamp(y, static_cast<double>(margin), static_cast<double>(height - margin))
            );
            
            if (firstPoint) {
                path.moveTo(pixelPoint);
                firstPoint = false;
            } else {
                path.lineTo(pixelPoint);
            }
        }
    } else {

        for (const auto& point : m_measurement.data) {
            const double logMag = GraphRenderer::calculateLogMag(point.s11);
            
            const double x = margin + (point.frequency - bounds.minFreq) * invFreqRange * plotWidth;
            const double y = height - margin - (logMag - bounds.minMag) * invMagRange * plotHeight;
            
            const QPointF pixelPoint(
                std::clamp(x, static_cast<double>(margin), static_cast<double>(width - margin)),
                std::clamp(y, static_cast<double>(margin), static_cast<double>(height - margin))
            );
            
            if (firstPoint) {
                path.moveTo(pixelPoint);
                firstPoint = false;
            } else {
                path.lineTo(pixelPoint);
            }
        }
    }
    

    painter->drawPath(path);
    

    if (dataSize < 500 ) {
        painter->setBrush(Qt::blue);
        const size_t step =std::max(size_t(1), dataSize / 500);
        
        for (size_t i = 0; i < dataSize; i += step) {
            const auto& point = m_measurement.data[i];
            const double logMag = GraphRenderer::calculateLogMag(point.s11);
            
            const double x = margin + (point.frequency - bounds.minFreq) * invFreqRange * plotWidth;
            const double y = height - margin - (logMag - bounds.minMag) * invMagRange * plotHeight;
            
            const QPointF pixelPoint(
                std::clamp(x, static_cast<double>(margin), static_cast<double>(width - margin)),
                std::clamp(y, static_cast<double>(margin), static_cast<double>(height - margin))
            );
            
            painter->drawEllipse(pixelPoint, 2, 2);
        }
    }
}
