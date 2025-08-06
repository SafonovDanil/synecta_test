#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <qDebug>
#include "Backend.h"
#include "GraphWidget.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<Backend>("com.s11analyzer", 1, 0, "Backend");
    qmlRegisterType<GraphWidget>("com.s11analyzer", 1, 0, "GraphWidget");
      
    QQmlApplicationEngine engine;
    
    QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    engine.load(url);
    
    if (engine.rootObjects().isEmpty()) {
        url = QUrl::fromLocalFile("../qml/Main.qml");
        engine.load(url);
    }
    
    if (engine.rootObjects().isEmpty()) {
        qFatal("Failed to load QML file");
    }

    return app.exec();
}
