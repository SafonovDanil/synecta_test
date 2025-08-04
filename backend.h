#pragma once

#include <QObject>
#include <QUrl>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include "Measurement.h"
#include "S11Parser.h"

class GraphWidget;

class FileParseWorker : public QObject {
    Q_OBJECT

public:
    struct ParseResult {
        S11Parser::ParseResult result;
        Measurement measurement;
        QString errorMessage;
    };

public slots:
    void parseFile(const QString& filePath);

signals:
    void parseCompleted(const FileParseWorker::ParseResult& result);
};

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(int dataPointCount READ dataPointCount NOTIFY dataPointCountChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();
    
    QString errorMessage() const { return m_errorMessage; }
    bool hasData() const { return m_hasData.load(); }
    bool isLoading() const { return m_isLoading.load(); }
    int dataPointCount() const { 
        std::shared_lock lock(m_dataMutex); 
        return static_cast<int>(m_measurement.size()); 
    }
    
    Q_INVOKABLE void setGraphWidget(GraphWidget* widget) { m_graphWidget = widget; }
    GraphWidget* getGraphWidget() const { return m_graphWidget; }

public slots:
    void loadFile(const QUrl& fileUrl);
    void clearData();

signals:
    void errorMessageChanged();
    void hasDataChanged();
    void isLoadingChanged();
    void dataPointCountChanged();
    void graphUpdated();

private slots:
    void onParseCompleted(const FileParseWorker::ParseResult& result);

private:
    void setErrorMessage(const QString& message);
    void setHasData(bool hasData);
    void setIsLoading(bool loading);
    
    QString m_errorMessage;
    std::atomic<bool> m_hasData{false};
    std::atomic<bool> m_isLoading{false};
    Measurement m_measurement;
    GraphWidget* m_graphWidget;
    
    std::unique_ptr<QThreadPool> m_threadPool;
    QThread* m_workerThread;
    FileParseWorker* m_worker;
    mutable std::shared_mutex m_dataMutex;
};
