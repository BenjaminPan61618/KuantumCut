#include "app/SettingsManager.h"

#include <QAudioDevice>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QPalette>
#include <QStyleHints>
#include <QQmlEngine>
#include <QVariantMap>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    m_language = m_settings.value(QStringLiteral("language"), QStringLiteral("zh_CN")).toString();
    m_theme = m_settings.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    m_defaultWidth = m_settings.value(QStringLiteral("defaultWidth"), 1920).toInt();
    m_defaultHeight = m_settings.value(QStringLiteral("defaultHeight"), 1080).toInt();
    m_mediaBackend = m_settings.value(QStringLiteral("mediaBackend"), QStringLiteral("auto")).toString();
    m_hardwareDecode = m_settings.value(QStringLiteral("hardwareDecode"), QStringLiteral("auto")).toString();
    m_audioOutput = m_settings.value(QStringLiteral("audioOutput")).toString();
    m_autosaveEnabled = m_settings.value(QStringLiteral("autosaveEnabled"), true).toBool();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &SettingsManager::darkModeChanged);
    applyTheme();
}

void SettingsManager::applyTheme() const
{
    QStyleHints *hints = QGuiApplication::styleHints();
    bool dark = false;
    if (m_theme == QStringLiteral("dark")) {
        hints->setColorScheme(Qt::ColorScheme::Dark);
        dark = true;
    } else if (m_theme == QStringLiteral("light")) {
        hints->setColorScheme(Qt::ColorScheme::Light);
    } else {
        hints->unsetColorScheme();
        dark = hints->colorScheme() == Qt::ColorScheme::Dark;
    }

    QPalette palette = QGuiApplication::palette();
    if (dark) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#18181b")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#f4f4f5")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#111113")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#27272a")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#f4f4f5")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#27272a")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f4f4f5")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#27272a")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f4f4f5")));
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#a1a1aa")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3b82f6")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    } else {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#f4f4f5")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#18181b")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#e4e4e7")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#18181b")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#e4e4e7")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#18181b")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#18181b")));
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#71717a")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2563eb")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    }
    QGuiApplication::setPalette(palette);
}

bool SettingsManager::darkMode() const
{
    if (m_theme == QStringLiteral("dark"))
        return true;
    if (m_theme == QStringLiteral("light"))
        return false;
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void SettingsManager::setLanguage(const QString &value)
{
    if (value != QStringLiteral("zh_CN") && value != QStringLiteral("en_US"))
        return;
    if (m_language == value)
        return;
    m_language = value;
    m_settings.setValue(QStringLiteral("language"), value);
    emit languageChanged();
}

void SettingsManager::setTheme(const QString &value)
{
    if (value != QStringLiteral("system") && value != QStringLiteral("light")
        && value != QStringLiteral("dark"))
        return;
    if (m_theme == value)
        return;
    m_theme = value;
    m_settings.setValue(QStringLiteral("theme"), value);
    applyTheme();
    emit themeChanged();
    emit darkModeChanged();
}

void SettingsManager::setDefaultWidth(int value)
{
    value = qBound(16, value, 16384);
    if (m_defaultWidth == value)
        return;
    m_defaultWidth = value;
    m_settings.setValue(QStringLiteral("defaultWidth"), value);
    emit defaultWidthChanged();
}

void SettingsManager::setDefaultHeight(int value)
{
    value = qBound(16, value, 16384);
    if (m_defaultHeight == value)
        return;
    m_defaultHeight = value;
    m_settings.setValue(QStringLiteral("defaultHeight"), value);
    emit defaultHeightChanged();
}

void SettingsManager::setMediaBackend(const QString &value)
{
    if (value != QStringLiteral("auto") && value != QStringLiteral("ffmpeg"))
        return;
    if (m_mediaBackend == value)
        return;
    m_mediaBackend = value;
    m_settings.setValue(QStringLiteral("mediaBackend"), value);
    emit mediaBackendChanged();
}

void SettingsManager::setHardwareDecode(const QString &value)
{
    if (value != QStringLiteral("auto") && value != QStringLiteral("disabled"))
        return;
    if (m_hardwareDecode == value)
        return;
    m_hardwareDecode = value;
    m_settings.setValue(QStringLiteral("hardwareDecode"), value);
    emit hardwareDecodeChanged();
}

void SettingsManager::setAudioOutput(const QString &value)
{
    if (m_audioOutput == value)
        return;
    m_audioOutput = value;
    m_settings.setValue(QStringLiteral("audioOutput"), value);
    emit audioOutputChanged();
}

void SettingsManager::setAutosaveEnabled(bool value)
{
    if (m_autosaveEnabled == value)
        return;
    m_autosaveEnabled = value;
    m_settings.setValue(QStringLiteral("autosaveEnabled"), value);
    emit autosaveEnabledChanged();
}

QVariantList SettingsManager::audioOutputDevices() const
{
    QVariantList result;
    result.append(QVariantMap{{QStringLiteral("id"), QString{}},
                              {QStringLiteral("description"), tr("System default")}});
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices) {
        result.append(QVariantMap{{QStringLiteral("id"), QString::fromLatin1(device.id())},
                                  {QStringLiteral("description"), device.description()}});
    }
    return result;
}

void SettingsManager::applyMediaEnvironment() const
{
    if (m_mediaBackend == QStringLiteral("ffmpeg"))
        qputenv("QT_MEDIA_BACKEND", QByteArrayLiteral("ffmpeg"));
    else
        qunsetenv("QT_MEDIA_BACKEND");

    qputenv("QT_FFMPEG_DECODING_HW",
            m_hardwareDecode == QStringLiteral("disabled") ? QByteArrayLiteral("0")
                                                            : QByteArrayLiteral("1"));
}

void SettingsManager::applyLanguage(QQmlEngine *engine)
{
    if (m_translationInstalled) {
        QCoreApplication::removeTranslator(&m_translator);
        m_translationInstalled = false;
    }

    if (m_language == QStringLiteral("en_US")
        && m_translator.load(QStringLiteral("kuantumcut_en_US"), QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&m_translator);
        m_translationInstalled = true;
    }
    if (engine)
        engine->retranslate();
}
