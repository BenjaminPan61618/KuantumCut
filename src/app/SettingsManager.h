#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QVariantList>

class QQmlEngine;

class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(int defaultWidth READ defaultWidth WRITE setDefaultWidth NOTIFY defaultWidthChanged)
    Q_PROPERTY(int defaultHeight READ defaultHeight WRITE setDefaultHeight NOTIFY defaultHeightChanged)
    Q_PROPERTY(QString mediaBackend READ mediaBackend WRITE setMediaBackend NOTIFY mediaBackendChanged)
    Q_PROPERTY(QString hardwareDecode READ hardwareDecode WRITE setHardwareDecode NOTIFY hardwareDecodeChanged)
    Q_PROPERTY(QString audioOutput READ audioOutput WRITE setAudioOutput NOTIFY audioOutputChanged)
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY darkModeChanged)
    Q_PROPERTY(bool autosaveEnabled READ autosaveEnabled WRITE setAutosaveEnabled NOTIFY autosaveEnabledChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);

    QString language() const { return m_language; }
    QString theme() const { return m_theme; }
    int defaultWidth() const { return m_defaultWidth; }
    int defaultHeight() const { return m_defaultHeight; }
    QString mediaBackend() const { return m_mediaBackend; }
    QString hardwareDecode() const { return m_hardwareDecode; }
    QString audioOutput() const { return m_audioOutput; }
    bool darkMode() const;
    bool autosaveEnabled() const { return m_autosaveEnabled; }

    void setLanguage(const QString &value);
    void setTheme(const QString &value);
    void setDefaultWidth(int value);
    void setDefaultHeight(int value);
    void setMediaBackend(const QString &value);
    void setHardwareDecode(const QString &value);
    void setAudioOutput(const QString &value);
    void setAutosaveEnabled(bool value);

    Q_INVOKABLE QVariantList audioOutputDevices() const;
    void applyMediaEnvironment() const;
    void applyLanguage(QQmlEngine *engine);

private:
    void applyTheme() const;

signals:
    void languageChanged();
    void themeChanged();
    void defaultWidthChanged();
    void defaultHeightChanged();
    void mediaBackendChanged();
    void hardwareDecodeChanged();
    void audioOutputChanged();
    void darkModeChanged();
    void autosaveEnabledChanged();

private:
    QSettings m_settings;
    QTranslator m_translator;
    bool m_translationInstalled = false;
    QString m_language;
    QString m_theme;
    int m_defaultWidth;
    int m_defaultHeight;
    QString m_mediaBackend;
    QString m_hardwareDecode;
    QString m_audioOutput;
    bool m_autosaveEnabled;
};
