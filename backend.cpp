#include "Backend.h"
#include "GraphWidget.h"
#include <QUrl>
#include <QThread>

void FileParseWorker::parseFile(const QString& filePath) {
    ParseResult result;
    
    result.result = S11Parser::parseFile(filePath.toStdString(), result.measurement);
    
    switch (result.result) {
        case S11Parser::ParseResult::Success:
            result.errorMessage = "";
            break;
        case S11Parser::ParseResult::FileNotFound:
            result.errorMessage = "File not found: " + filePath;
            break;
        case S11Parser::ParseResult::InvalidFormat:
            result.errorMessage = "Invalid Touchstone file format. Expected format: # Hz S RI R 50";
            break;
        case S11Parser::ParseResult::EmptyFile:
            result.errorMessage = "File contains no valid data points";
            break;
    }
    
    emit parseCompleted(result);
}

Backend::Backend(QObject *parent)
    : QObject(parent)
    , m_graphWidget(nullptr)
    , m_threadPool(std::make_unique<QThreadPool>())
    , m_workerThread(nullptr)
    , m_worker(nullptr) {
    
    const int idealThreadCount = std::max(2, QThread::idealThreadCount());
    m_threadPool->setMaxThreadCount(idealThreadCount);
    
    m_workerThread = new QThread(this);
    m_worker = new FileParseWorker();
    m_worker->moveToThread(m_workerThread);
    
    connect(m_worker, &FileParseWorker::parseCompleted,
            this, &Backend::onParseCompleted, Qt::QueuedConnection);
    
    m_workerThread->start();
}

Backend::~Backend() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);

        if (m_worker) {
            delete m_worker;
        }
    }
}

void Backend::loadFile(const QUrl& fileUrl) {
    QString filePath = fileUrl.toLocalFile();
    
    if (filePath.isEmpty()) {
        setErrorMessage("Invalid file path");
        return;
    }
    
    if (m_isLoading) {
        return; // Already loading
    }
    
    setIsLoading(true);
    setErrorMessage("");
    
    // Parse file in worker thread
    QMetaObject::invokeMethod(m_worker, "parseFile", Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
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



void Backend::onParseCompleted(const FileParseWorker::ParseResult& result) {
    setIsLoading(false);
    
    if (result.result == S11Parser::ParseResult::Success) {
        {
            std::unique_lock lock(m_dataMutex);
            m_measurement = std::move(const_cast<Measurement&>(result.measurement));
        }
        
        setErrorMessage("");
        setHasData(true);
        emit dataPointCountChanged();
        
        if (m_graphWidget) {
            m_graphWidget->updateMeasurement(m_measurement);
        }
        emit graphUpdated();
    } else {
        setErrorMessage(result.errorMessage);
        setHasData(false);
        emit dataPointCountChanged();
    }
}

