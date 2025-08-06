#include "GraphWidget.h"
#include "PerformanceUtils.h"
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
    //PERF_MEASURE("GraphWidget::paint");
    
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
    
    const auto bounds = GraphRenderer::calculateBounds(m_measurement, m_zoomParams);
    const double freqRange = bounds.maxFreq - bounds.minFreq;
    const double magRange = bounds.maxMag - bounds.minMag;
    
    if (freqRange <= 0 || magRange <= 0) return;
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Отрисовка сетки
    const QPen gridPen(Qt::lightGray, 1, Qt::DotLine);
    painter->setPen(gridPen);
    
    // X
    for (int i = 1; i < 10; ++i) {
        const int x = margin + i * plotWidth / 10;
        painter->drawLine(x, margin, x, height - margin);
    }
    
    // Y
    for (int i = 1; i < 10; ++i) {
        const int y = margin + i * plotHeight / 10;
        painter->drawLine(margin, y, width - margin, y);
    }
    
    // Отрисовка осей
    const QPen axisPen(Qt::black, 2);
    painter->setPen(axisPen);
    painter->drawLine(margin, height - margin, width - margin, height - margin); // X
    painter->drawLine(margin, margin, margin, height - margin); // Y
    
    // Подпись осей
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
    
    // Подпись значений на осях
    constexpr int numTicks = 5;
    const double freqStep = freqRange / numTicks;
    const double magStep = magRange / numTicks;
    
    // X
    for (int i = 0; i <= numTicks; ++i) {
        const double freq = bounds.minFreq + i * freqStep;
        const int x = margin + i * plotWidth / numTicks;
        
        QString freqStr = formatFrequency(freq);
        painter->drawText(x - 20, height - margin + 20, freqStr);
    }
    
    // Y
    for (int i = 0; i <= numTicks; ++i) {
        const double mag = bounds.minMag + i * magStep;
        const int y = height - margin - i * plotHeight / numTicks;
        
        const QString magStr = QString::number(mag, 'f', 1);
        painter->drawText(5, y + 5, magStr);
    }
    
    // Отрисовка графика
    drawDataPoints(painter, bounds, width, height, margin);
    
    lock.unlock();
}

// Хэндлеры

void GraphWidget::updateMeasurement(const Measurement& measurement) {
    {
        std::unique_lock lock(m_dataMutex);
        m_measurement = measurement;
    }
    
    setHasData(!measurement.empty());
    update();
}

void GraphWidget::setZoomParams(const GraphRenderer::ZoomParams& zoom) {
    bool wasActive;
    {
        std::unique_lock lock(m_dataMutex);
        wasActive = m_zoomParams.isActive;
        m_zoomParams = zoom;
    }
    
    if (wasActive != zoom.isActive) {
        emit isZoomedChanged();
    }
    
    update();
}

void GraphWidget::resetZoom() {
    {
        std::unique_lock lock(m_dataMutex);
        m_zoomParams.isActive = false;
    }
    
    emit isZoomedChanged();
    update();
}

void GraphWidget::setIsLoading(bool loading) {
    const bool oldValue = m_isLoading.exchange(loading);
    if (oldValue != loading) {
        emit isLoadingChanged();
        update();
    }
}

void GraphWidget::setLoadingText(const QString& text) {
    if (m_loadingText != text) {
        m_loadingText = text;
        emit loadingTextChanged();
        if (m_isLoading) {
            update();
        }
    }
}

void GraphWidget::setEmptyText(const QString& text) {
    if (m_emptyText != text) {
        m_emptyText = text;
        emit emptyTextChanged();
        if (!m_hasData && !m_isLoading) {
            update(); 
        }
    }
}

void GraphWidget::setHasData(bool hasData) {
    const bool oldValue = m_hasData.exchange(hasData);
    if (oldValue != hasData) {
        emit hasDataChanged();
    }
}

void GraphWidget::drawLoadingOverlay(QPainter *painter) {
    painter->fillRect(0, 0, width(), height(), QColor(255, 255, 255, 200));
    
    painter->setPen(Qt::black);
    QFont font = painter->font();
    font.setPointSize(14);
    painter->setFont(font);
    
    const QRectF textRect(0, 0, width(), height());
    painter->drawText(textRect, Qt::AlignCenter, m_loadingText);
}

void GraphWidget::drawEmptyState(QPainter *painter) {
    painter->setPen(QColor(102, 102, 102));
    QFont font = painter->font();
    font.setPointSize(14);
    painter->setFont(font);
    
    const QRectF textRect(0, 0, width(), height());
    painter->drawText(textRect, Qt::AlignCenter, m_emptyText);
}


QString GraphWidget::formatFrequency(double freq) const {
    if (freq >= 1e9) {
        return QString::number(freq / 1e9, 'f', 1) + "G";   // Гига
    } else if (freq >= 1e6) {
        return QString::number(freq / 1e6, 'f', 1) + "M";   // Мега
    } else if (freq >= 1e3) {
        return QString::number(freq / 1e3, 'f', 1) + "k";   // Кило
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
        
    // Отрисовка графика с помощью кривой QPainterPath
    QPainterPath path;
    bool firstPoint = true;
    
    const size_t dataSize = m_measurement.data.size();
    // Аппроксимация, если точек много
    if (dataSize > 1000) {
        const size_t step = std::max(size_t(1), dataSize / 2000);
        
        for (size_t i = 0; i < dataSize; i += step) {
            const auto& point = m_measurement.data[i];
            const double logMag = GraphRenderer::calculateLogMag(point.s11);
            
            const double x = margin + (point.frequency - bounds.minFreq) * invFreqRange * plotWidth;
            const double y = height - margin - (logMag - bounds.minMag) * invMagRange * plotHeight;
            
            // Сжатие, если одна из точек вышла за границы
            const double clampedX = std::clamp(x, -1000.0, static_cast<double>(width + 1000));
            const double clampedY = std::clamp(y, -1000.0, static_cast<double>(height + 1000));
            
            const QPointF pixelPoint(clampedX, clampedY);
            
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
            
           // Сжатие, если одна из точек вышла за границы
            const double clampedX = std::clamp(x, -1000.0, static_cast<double>(width + 1000));
            const double clampedY = std::clamp(y, -1000.0, static_cast<double>(height + 1000));
            
            const QPointF pixelPoint(clampedX, clampedY);
            
            if (firstPoint) {
                path.moveTo(pixelPoint);
                firstPoint = false;
            } else {
                path.lineTo(pixelPoint);
            }
        }
    }
    painter->drawPath(path);
    
    // Отрисовка точек
    if (dataSize < 500 || m_zoomParams.isActive) {
        painter->setBrush(Qt::blue);
        const size_t step = m_zoomParams.isActive ? 1 : std::max(size_t(1), dataSize / 500);
        
        for (size_t i = 0; i < dataSize; i += step) {
            const auto& point = m_measurement.data[i];
            const double logMag = GraphRenderer::calculateLogMag(point.s11);
            
            const double x = margin + (point.frequency - bounds.minFreq) * invFreqRange * plotWidth;
            const double y = height - margin - (logMag - bounds.minMag) * invMagRange * plotHeight;
            
            if (x >= margin && x <= width - margin && y >= margin && y <= height - margin) {
                const QPointF pixelPoint(x, y);
                painter->drawEllipse(pixelPoint, 2, 2);
            }
        }
    }
}
