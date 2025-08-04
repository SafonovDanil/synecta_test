#pragma once

#include <QQuickPaintedItem>
#include <QPainter>
#include <QMouseEvent>
#include <QPointF>
#include <atomic>
#include <shared_mutex>
#include "Measurement.h"
#include "GraphRenderer.h"

class GraphWidget : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
    Q_PROPERTY(bool isLoading READ isLoading WRITE setIsLoading NOTIFY isLoadingChanged)


public:
    explicit GraphWidget(QQuickItem *parent = nullptr);
    
    bool hasData() const { return m_hasData.load(); }
    bool isLoading() const { return m_isLoading.load(); }
    QString loadingText() const { return m_loadingText; }
    QString emptyText() const { return m_emptyText; }
    
    void setIsLoading(bool loading);


public slots:
    void updateMeasurement(const Measurement& measurement);

signals:
    void hasDataChanged();
    void isLoadingChanged();
    void loadingTextChanged();
    void emptyTextChanged();


protected:
    void paint(QPainter *painter) override;

private:
    void setHasData(bool hasData);
    void drawLoadingOverlay(QPainter *painter);
    void drawEmptyState(QPainter *painter);
    void drawDataPoints(QPainter *painter, const GraphRenderer::GraphBounds& bounds, 
                       int width, int height, int margin);
    QString formatFrequency(double freq) const;
    
    Measurement m_measurement;
    mutable std::shared_mutex m_dataMutex;
    
    std::atomic<bool> m_hasData{false};
    std::atomic<bool> m_isLoading{false};
    QString m_loadingText = "Loading graph...";
    QString m_emptyText = "Load a Touchstone file to display S11 graph";
};
