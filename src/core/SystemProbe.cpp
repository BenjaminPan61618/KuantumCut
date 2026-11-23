#include "core/SystemProbe.h"

#include <QFile>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>

#include <utility>

SystemProbe::SystemProbe(QObject *parent)
    : QObject(parent)
{
    m_cpuCount = QThread::idealThreadCount();
#ifdef Q_OS_LINUX
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!meminfo.atEnd()) {
            const QByteArray line = meminfo.readLine();
            if (line.startsWith("MemTotal:")) {
                const QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() >= 2)
                    m_memoryBytes = parts.at(1).toLongLong() * 1024;
                break;
            }
        }
    }
#endif
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(3000);
    connect(&m_timeout, &QTimer::timeout, this, &SystemProbe::finishSample);
}

void SystemProbe::start(QQuickWindow *window)
{
    for (const QMetaObject::Connection &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    m_timeout.stop();
    m_window = window;
    m_frames = 0;
    m_slowFrames = 0;
    m_sampleMilliseconds = 0;
    m_framesPerSecond = 0.0;
    m_rendererQueried = false;
    m_softwareRendering = false;
    emit framesPerSecondChanged();
    emit softwareRenderingChanged();

    if (!window) {
        m_renderer = QStringLiteral("Unavailable");
        m_renderApi = QStringLiteral("Unknown");
        m_level = QStringLiteral("compatibility");
        m_score = 0;
        m_details = QStringLiteral("No QQuickWindow is available for rendering measurements.");
        emit rendererChanged();
        emit renderApiChanged();
        emit levelChanged();
        emit scoreChanged();
        emit detailsChanged();
        setBusy(false);
        return;
    }

    QSGRendererInterface *rendererInterface = window->rendererInterface();
    const QSGRendererInterface::GraphicsApi api = rendererInterface
        ? rendererInterface->graphicsApi() : QQuickWindow::graphicsApi();
    switch (api) {
    case QSGRendererInterface::OpenGL:
        m_renderApi = QStringLiteral("OpenGL");
        m_renderer = QStringLiteral("OpenGL (querying renderer)");
        break;
    case QSGRendererInterface::Direct3D11:
        m_renderApi = QStringLiteral("Direct3D11");
        m_renderer = m_renderApi;
        break;
    case QSGRendererInterface::Direct3D12:
        m_renderApi = QStringLiteral("Direct3D12");
        m_renderer = m_renderApi;
        break;
    case QSGRendererInterface::Vulkan:
        m_renderApi = QStringLiteral("Vulkan");
        m_renderer = m_renderApi;
        break;
    case QSGRendererInterface::Metal:
        m_renderApi = QStringLiteral("Metal");
        m_renderer = m_renderApi;
        break;
    case QSGRendererInterface::Software:
        m_renderApi = QStringLiteral("Software");
        m_renderer = QStringLiteral("Qt software renderer");
        m_softwareRendering = true;
        break;
    default:
        m_renderApi = QStringLiteral("Unknown");
        m_renderer = QStringLiteral("Unknown renderer");
        break;
    }
    emit rendererChanged();
    emit renderApiChanged();
    emit softwareRenderingChanged();
    updateDetails();
    setBusy(true);

    if (api == QSGRendererInterface::OpenGL) {
        m_connections << connect(window, &QQuickWindow::beforeRendering, this, [this]() {
            if (m_rendererQueried)
                return;
            QOpenGLContext *context = QOpenGLContext::currentContext();
            if (!context)
                return;
            const GLubyte *value = context->functions()->glGetString(GL_RENDERER);
            if (!value)
                return;
            m_rendererQueried = true;
            const QString name = QString::fromUtf8(reinterpret_cast<const char *>(value));
            QMetaObject::invokeMethod(this, [this, name]() { setRenderer(name); },
                                      Qt::QueuedConnection);
        }, Qt::DirectConnection);
    }

    m_connections << connect(window, &QQuickWindow::frameSwapped, this, [this]() {
        if (!m_busy)
            return;
        if (!m_elapsed.isValid()) {
            m_elapsed.start();
            return;
        }
        const qint64 interval = m_elapsed.restart();
        if (interval <= 0)
            return;
        ++m_frames;
        m_sampleMilliseconds += interval;
        if (interval > 50)
            ++m_slowFrames;
        if (m_frames >= 60 || (m_sampleMilliseconds >= 1500 && m_frames >= 15))
            finishSample();
    });
    m_timeout.start();
    window->update();
}

void SystemProbe::finishSample()
{
    if (!m_busy)
        return;
    m_timeout.stop();
    for (const QMetaObject::Connection &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    if (m_sampleMilliseconds > 0)
        m_framesPerSecond = m_frames * 1000.0 / m_sampleMilliseconds;
    emit framesPerSecondChanged();
    updateScore();
    updateDetails();
    setBusy(false);
}

void SystemProbe::updateDetails()
{
    QStringList lines;
    lines << QStringLiteral("Render API: %1").arg(m_renderApi);
    lines << QStringLiteral("Renderer: %1").arg(m_renderer);
    lines << QStringLiteral("Logical CPU threads: %1").arg(m_cpuCount);
    if (m_memoryBytes > 0)
        lines << QStringLiteral("System memory: %1 GiB").arg(m_memoryBytes / 1073741824.0, 0, 'f', 1);
    else
        lines << QStringLiteral("System memory: unavailable on this platform");
    if (m_frames > 0) {
        lines << QStringLiteral("Frame sample: %1 frames, %2 FPS average, %3 slow frames")
                     .arg(m_frames).arg(m_framesPerSecond, 0, 'f', 1).arg(m_slowFrames);
    } else if (!m_busy) {
        lines << QStringLiteral("Frame sample: no frame intervals were received");
    }
    if (m_softwareRendering)
        lines << QStringLiteral("Software rasterizer detected; compatibility mode is required.");
    lines << QStringLiteral("This is a short preview estimate, not a hardware decoding capability test.");
    m_details = lines.join(u'\n');
    emit detailsChanged();
}

void SystemProbe::updateScore()
{
    int value = 40;
    if (m_framesPerSecond >= 55.0)
        value = 90;
    else if (m_framesPerSecond >= 45.0)
        value = 75;
    else if (m_framesPerSecond >= 28.0)
        value = 55;
    else if (m_framesPerSecond > 0.0)
        value = 35;
    if (m_cpuCount > 0 && m_cpuCount < 2)
        value -= 10;
    if (m_memoryBytes > 0 && m_memoryBytes < 4LL * 1024 * 1024 * 1024)
        value -= 10;
    if (m_softwareRendering)
        value = qMin(value, 35);
    value = qBound(0, value, 100);
    if (m_score != value) {
        m_score = value;
        emit scoreChanged();
    }

    const QString newLevel = m_softwareRendering || value < 50
        ? QStringLiteral("compatibility")
        : value < 75 ? QStringLiteral("basic") : QStringLiteral("smooth");
    if (m_level != newLevel) {
        m_level = newLevel;
        emit levelChanged();
    }
}

void SystemProbe::setRenderer(const QString &renderer)
{
    if (renderer.isEmpty())
        return;
    m_renderer = renderer;
    const QString lower = renderer.toLower();
    const bool software = lower.contains(QStringLiteral("llvmpipe"))
        || lower.contains(QStringLiteral("softpipe")) || lower.contains(QStringLiteral("swiftshader"))
        || lower.contains(QStringLiteral("lavapipe")) || lower.contains(QStringLiteral("software"))
        || lower.contains(QStringLiteral("microsoft basic render"));
    if (m_softwareRendering != software) {
        m_softwareRendering = software;
        emit softwareRenderingChanged();
    }
    emit rendererChanged();
    if (!m_busy)
        updateScore();
    updateDetails();
}

void SystemProbe::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
