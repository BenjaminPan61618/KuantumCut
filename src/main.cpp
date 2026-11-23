#include "app/SettingsManager.h"
#include "core/DraftRepository.h"
#include "core/SystemProbe.h"
#include "editor/EditorController.h"
#include "editor/ExportController.h"
#include "editor/PlaybackController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("KuantumCut"));
    QCoreApplication::setApplicationName(QStringLiteral("KuantumCut"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    SettingsManager settings;
    settings.applyMediaEnvironment();

    SystemProbe probe;
    DraftRepository drafts;
    EditorController editor(&drafts);
    editor.playback()->setAudioOutput(settings.audioOutput());
    ExportController exporter(&editor);

    QQmlApplicationEngine engine;
    settings.applyLanguage(&engine);
    QObject::connect(&settings, &SettingsManager::languageChanged, &engine,
                     [&settings, &engine]() { settings.applyLanguage(&engine); });
    QObject::connect(&settings, &SettingsManager::audioOutputChanged, editor.playback(),
                     [&settings, &editor]() { editor.playback()->setAudioOutput(settings.audioOutput()); });
    editor.setAutosaveEnabled(settings.autosaveEnabled());
    QObject::connect(&settings, &SettingsManager::autosaveEnabledChanged, &editor,
                     [&settings, &editor]() { editor.setAutosaveEnabled(settings.autosaveEnabled()); });

    QQmlContext *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("app"), &application);
    context->setContextProperty(QStringLiteral("settings"), &settings);
    context->setContextProperty(QStringLiteral("probe"), &probe);
    context->setContextProperty(QStringLiteral("drafts"), &drafts);
    context->setContextProperty(QStringLiteral("editor"), &editor);
    context->setContextProperty(QStringLiteral("exporter"), &exporter);
    context->setContextProperty(QStringLiteral("playback"), editor.playback());
    context->setContextProperty(QStringLiteral("assets"), editor.assets());
    context->setContextProperty(QStringLiteral("timeline"), editor.timeline());
    context->setContextProperty(QStringLiteral("activeVisuals"), editor.playback()->activeVisuals());

    const QUrl mainUrl(QStringLiteral("qrc:/qt/qml/KuantumCut/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &application,
                     [mainUrl](QObject *object, const QUrl &url) {
                         if (!object && url == mainUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty())
        return -1;
    return QGuiApplication::exec();
}
