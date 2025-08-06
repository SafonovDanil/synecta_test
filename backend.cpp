#include "Backend.h"
#include "GraphWidget.h"
#include <QUrl>
#include <QFutureWatcher>
#include <QDebug>
#include <tuple>

static std::tuple<S11Parser::ParseResult, Measurement, QString> parseFileAsync(const QString& filePath) {
    Measurement measurement;
    S11Parser::ParseResult result = S11Parser::parseFile(filePath.toStdString(), measurement);
    
    QString errorMessage;
    switch (result) {
        case S11Parser::ParseResult::Success:
            errorMessage = "";
            break;
        case S11Parser::ParseResult::FileNotFound:
            errorMessage = "File not found: " + filePath;
            break;
        case S11Parser::ParseResult::InvalidFormat:
            errorMessage = "Invalid Touchstone file format. Expected format: # Hz S RI R 50";
            break;
        case S11Parser::ParseResult::EmptyFile:
            errorMessage = "File contains no valid data points";
            break;
    }
    
    return std::make_tuple(result, std::move(measurement), errorMessage);
}

Backend::Backend(QObject *parent)
    : QObject(parent)
    , m_graphWidget(nullptr)
    , m_threadPool(std::make_unique<QThreadPool>()) {
    
    const int idealThreadCount = std::max(2, QThread::idealThreadCount());
    m_threadPool->setMaxThreadCount(idealThreadCount);
}

Backend::~Backend() {
    // Есть QThreadPool
}

void Backend::loadFile(const QUrl& fileUrl) {
    QString filePath = fileUrl.toLocalFile();
    
    if (filePath.isEmpty()) {
        setErrorMessage("Invalid file path");
        return;
    }
    
    if (!filePath.toLower().endsWith(".s1p")) {
        setErrorMessage("Unsupported file format. Please select a Touchstone (.s1p) file.");
        return;
    }
    
    if (m_isLoading) {
        return;
    }
    
    setIsLoading(true);
    setErrorMessage("");
    
    auto future = QtConcurrent::run(m_threadPool.get(), parseFileAsync, filePath);
    auto watcher = new QFutureWatcher<std::tuple<S11Parser::ParseResult, Measurement, QString>>(this);
    
    connect(watcher, &QFutureWatcher<std::tuple<S11Parser::ParseResult, Measurement, QString>>::finished,
            this, [this, watcher]() {
                auto result = watcher->result();
                onParseCompleted(std::get<0>(result), std::move(std::get<1>(result)), std::get<2>(result));
                watcher->deleteLater();
            });
    
    watcher->setFuture(future);
}

void Backend::clearData() {
    {
        std::unique_lock lock(m_dataMutex);
        m_measurement.clear();
    }
    
    setHasData(false);
    setErrorMessage("");
    emit dataPointCountChanged();
    
    if (m_graphWidget) {
        m_graphWidget->updateMeasurement(m_measurement);
        m_graphWidget->setZoomParams(m_zoomParams);
    }
    emit graphUpdated();
}

void Backend::setErrorMessage(const QString& message) {
    if (m_errorMessage != message) {
        m_errorMessage = message;
        emit errorMessageChanged();
    }
}

void Backend::setHasData(bool hasData) {
    const bool oldValue = m_hasData.exchange(hasData);
    if (oldValue != hasData) {
        emit hasDataChanged();
    }
}

void Backend::setIsLoading(bool loading) {
    const bool oldValue = m_isLoading.exchange(loading);
    if (oldValue != loading) {
        if (m_graphWidget) {
            m_graphWidget->setIsLoading(loading);
        }
        emit isLoadingChanged();
    }
}

void Backend::setIsZoomed(bool zoomed) {
    if (m_zoomParams.isActive != zoomed) {
        m_zoomParams.isActive = zoomed;
        emit isZoomedChanged();
    }
}

void Backend::zoomToRegion(double freqMin, double freqMax, double magMin, double magMax) {
    if (freqMin >= freqMax || magMin >= magMax || 
        freqMin < 0 || freqMax < 0 || 
        std::isnan(freqMin) || std::isnan(freqMax) || std::isnan(magMin) || std::isnan(magMax)) {
        qDebug() << "Invalid zoom parameters - ignoring";
        return;
    }
    
    const bool wasZoomed = m_zoomParams.isActive;
    
    m_zoomParams.freqMin = freqMin;
    m_zoomParams.freqMax = freqMax;
    m_zoomParams.magMin = magMin;
    m_zoomParams.magMax = magMax;
    m_zoomParams.isActive = true;
    
    qDebug() << "Setting zoom to: freq(" << freqMin << "-" << freqMax << ") mag(" << magMin << "-" << magMax << ")";
    
    if (!wasZoomed) {
        emit isZoomedChanged();
    }
    
    if (m_graphWidget) {
        m_graphWidget->setZoomParams(m_zoomParams);
    }
    emit graphUpdated();
}

void Backend::resetZoom() {
    const bool wasZoomed = m_zoomParams.isActive;
    
    m_zoomParams.isActive = false;
    m_zoomParams.freqMin = 0.0;
    m_zoomParams.freqMax = 0.0;
    m_zoomParams.magMin = 0.0;
    m_zoomParams.magMax = 0.0;
    
    qDebug() << "Zoom reset";
    
    if (wasZoomed) {
        emit isZoomedChanged();
    }
    
    if (m_graphWidget) {
        m_graphWidget->setZoomParams(m_zoomParams);
    }
    emit graphUpdated();
}

void Backend::zoomToPixelRegion(int x1, int y1, int x2, int y2, int imageWidth, int imageHeight) {
    std::shared_lock lock(m_dataMutex);
    
    if (m_measurement.empty()) {
        return;
    }

    GraphRenderer::ZoomParams noZoom;

    const auto originalBounds = GraphRenderer::calculateBounds(m_measurement, noZoom);
    
    const auto currentBounds = GraphRenderer::calculateBounds(m_measurement, m_zoomParams);
    
    constexpr int margin = 60;
    
    const auto [minX, maxX] = std::minmax(x1, x2);
    const auto [minY, maxY] = std::minmax(y1, y2);
    
    const double freqRange = currentBounds.maxFreq - currentBounds.minFreq;
    const double magRange = currentBounds.maxMag - currentBounds.minMag;
    const double plotWidth = imageWidth - 2 * margin;
    const double plotHeight = imageHeight - 2 * margin;
    
    const double freqMin = currentBounds.minFreq + (minX - margin) * freqRange / plotWidth;
    const double freqMax = currentBounds.minFreq + (maxX - margin) * freqRange / plotWidth;
    
    const double magMax = currentBounds.maxMag - (minY - margin) * magRange / plotHeight;
    const double magMin = currentBounds.maxMag - (maxY - margin) * magRange / plotHeight;
    
    // Сжатие границ на основе границ до зума
    const double clampedFreqMin = std::clamp(freqMin, originalBounds.minFreq, originalBounds.maxFreq);
    const double clampedFreqMax = std::clamp(freqMax, originalBounds.minFreq, originalBounds.maxFreq);
    const double clampedMagMin = std::clamp(magMin, originalBounds.minMag, originalBounds.maxMag);
    const double clampedMagMax = std::clamp(magMax, originalBounds.minMag, originalBounds.maxMag);
    
    qDebug() << "Zoom selection: pixels(" << x1 << "," << y1 << "," << x2 << "," << y2 << ") size(" << imageWidth << "x" << imageHeight << ")";
    qDebug() << "Current bounds: freq(" << currentBounds.minFreq << "-" << currentBounds.maxFreq << ") mag(" << currentBounds.minMag << "-" << currentBounds.maxMag << ")";
    qDebug() << "Calculated zoom: freq(" << clampedFreqMin << "-" << clampedFreqMax << ") mag(" << clampedMagMin << "-" << clampedMagMax << ")";
    
    if (clampedFreqMax > clampedFreqMin && clampedMagMax > clampedMagMin) {
        lock.unlock();
        zoomToRegion(clampedFreqMin, clampedFreqMax, clampedMagMin, clampedMagMax);
    } else {
        qDebug() << "Invalid zoom region - skipping";
    }
}

void Backend::onParseCompleted(S11Parser::ParseResult result, Measurement measurement, QString errorMessage) {
    setIsLoading(false);
    
    if (result == S11Parser::ParseResult::Success) {
        {
            std::unique_lock lock(m_dataMutex);
            m_measurement = std::move(measurement);
        }
        
        setErrorMessage("");
        setHasData(true);
        emit dataPointCountChanged();
        
        if (m_graphWidget) {
            m_graphWidget->updateMeasurement(m_measurement);
            m_graphWidget->setZoomParams(m_zoomParams);
        }
        emit graphUpdated();
    } else {
        setErrorMessage(errorMessage);
        setHasData(false);
        emit dataPointCountChanged();
    }
}

