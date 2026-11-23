#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QQuickWindow;

class SystemProbe : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString level READ level NOTIFY levelChanged)
    Q_PROPERTY(int score READ score NOTIFY scoreChanged)
    Q_PROPERTY(QString renderer READ renderer NOTIFY rendererChanged)
    Q_PROPERTY(QString details READ details NOTIFY detailsChanged)
    Q_PROPERTY(QString renderApi READ renderApi NOTIFY renderApiChanged)
    Q_PROPERTY(double framesPerSecond READ framesPerSecond NOTIFY framesPerSecondChanged)
    Q_PROPERTY(bool softwareRendering READ softwareRendering NOTIFY softwareRenderingChanged)

public:
    explicit SystemProbe(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString level() const { return m_level; }
    int score() const { return m_score; }
    QString renderer() const { return m_renderer; }
    QString details() const { return m_details; }
    QString renderApi() const { return m_renderApi; }
    double framesPerSecond() const { return m_framesPerSecond; }
    bool softwareRendering() const { return m_softwareRendering; }

    Q_INVOKABLE void start(QQuickWindow *window);

signals:
    void busyChanged();
    void levelChanged();
    void scoreChanged();
    void rendererChanged();
    void detailsChanged();
    void renderApiChanged();
    void framesPerSecondChanged();
    void softwareRenderingChanged();

private:
    void finishSample();
    void updateDetails();
    void updateScore();
    void setRenderer(const QString &renderer);
    void setBusy(bool busy);

    QPointer<QQuickWindow> m_window;
    QList<QMetaObject::Connection> m_connections;
    QTimer m_timeout;
    QElapsedTimer m_elapsed;
    bool m_busy = false;
    bool m_rendererQueried = false;
    int m_frames = 0;
    int m_slowFrames = 0;
    qint64 m_sampleMilliseconds = 0;
    int m_cpuCount = 0;
    qint64 m_memoryBytes = 0;
    double m_framesPerSecond = 0.0;
    QString m_level = QStringLiteral("unknown");
    int m_score = 0;
    QString m_renderer = QStringLiteral("Not measured");
    QString m_details = QStringLiteral("Start the probe to measure this system.");
    QString m_renderApi = QStringLiteral("Unknown");
    bool m_softwareRendering = false;
};
