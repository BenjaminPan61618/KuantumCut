#include "core/DraftRepository.h"
#include "models/AssetModel.h"
#include "models/TimelineModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

class TestDraft : public QObject
{
    Q_OBJECT

private slots:
    void roundTripRelativePathsAndMissingMedia();
    void renameAndDelete();
    void rejectClipBeyondMediaSource();
};

void TestDraft::roundTripRelativePathsAndMissingMedia()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString repositoryPath = temporary.filePath(QStringLiteral("library"));
    DraftRepository repository(repositoryPath);
    const QVariantMap created = repository.create(QStringLiteral("Round trip"), 1280, 720);
    QVERIFY(!created.isEmpty());
    const QString id = created.value(QStringLiteral("id")).toString();

    QFile sourceImage(temporary.filePath(QStringLiteral("picture.png")));
    QVERIFY(sourceImage.open(QIODevice::WriteOnly));
    QVERIFY(sourceImage.write("not-a-real-image-but-a-valid-test-file") > 0);
    sourceImage.close();

    AssetModel assets;
    TimelineModel timeline(&assets);
    Draft draft;
    QString error;
    QVERIFY2(repository.load(id, draft, assets, timeline, &error), qPrintable(error));
    const QString imageId = assets.importMedia(QUrl::fromLocalFile(sourceImage.fileName()));
    QVERIFY(!imageId.isEmpty());
    const QString textId = assets.addText(QStringLiteral("Caption"));
    QVERIFY(!textId.isEmpty());
    const QString relativePath = assets.relativeSource(imageId);
    QVERIFY(relativePath.startsWith(QStringLiteral("media/")));
    QVERIFY(!QDir::isAbsolutePath(relativePath));
    QVERIFY(QFileInfo::exists(assets.absoluteSource(imageId)));

    const QString imageClip = timeline.addClip(imageId, QStringLiteral("V1"), 0, 4000);
    QVERIFY(!imageClip.isEmpty());
    const QString overlay = timeline.addTrack();
    const QString textClip = timeline.addClip(textId, overlay, 500, 3000);
    QVERIFY(!textClip.isEmpty());
    QVERIFY(timeline.setClipProperty(textClip, QStringLiteral("scale"), 1.25));
    QVERIFY(timeline.setClipProperty(textClip, QStringLiteral("cropLeft"), 0.1));
    QVERIFY2(repository.save(draft, assets, timeline, &error), qPrintable(error));

    QFile draftFile(repository.draftFilePath(id));
    QVERIFY(draftFile.open(QIODevice::ReadOnly));
    const QJsonObject savedObject = QJsonDocument::fromJson(draftFile.readAll()).object();
    QCOMPARE(savedObject.value(QStringLiteral("format")).toString(), QStringLiteral("KuantumCutDraft"));
    QCOMPARE(savedObject.value(QStringLiteral("schemaVersion")).toInt(), Draft::SchemaVersion);
    const QJsonArray savedAssets = savedObject.value(QStringLiteral("assets")).toArray();
    QVERIFY(!savedAssets.isEmpty());
    QVERIFY(savedAssets.at(0).toObject().value(QStringLiteral("source")).toString()
                .startsWith(QStringLiteral("media/")));

    AssetModel loadedAssets;
    TimelineModel loadedTimeline(&loadedAssets);
    Draft loadedDraft;
    QVERIFY2(repository.load(id, loadedDraft, loadedAssets, loadedTimeline, &error), qPrintable(error));
    QCOMPARE(loadedDraft.name, draft.name);
    QCOMPARE(loadedDraft.width, 1280);
    QCOMPARE(loadedDraft.height, 720);
    QCOMPARE(loadedAssets.toJson(), assets.toJson());
    QCOMPARE(loadedTimeline.toJson(), timeline.toJson());
    QCOMPARE(loadedTimeline.getClip(textClip).value(QStringLiteral("scale")).toDouble(), 1.25);
    QCOMPARE(loadedTimeline.getClip(textClip).value(QStringLiteral("cropLeft")).toDouble(), 0.1);

    QVERIFY(QFile::remove(loadedAssets.absoluteSource(imageId)));
    loadedAssets.refreshMissing();
    QVERIFY(loadedAssets.missing(imageId));
    QVERIFY(!loadedAssets.missing(textId));

    QFile replacement(temporary.filePath(QStringLiteral("replacement.jpg")));
    QVERIFY(replacement.open(QIODevice::WriteOnly));
    QVERIFY(replacement.write("replacement") > 0);
    replacement.close();
    QVERIFY(loadedAssets.relocate(imageId, QUrl::fromLocalFile(replacement.fileName())));
    QVERIFY(!loadedAssets.missing(imageId));
    QVERIFY(loadedAssets.relativeSource(imageId).endsWith(QStringLiteral(".jpg")));
}

void TestDraft::renameAndDelete()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    const QVariantMap created = repository.create(QStringLiteral("Before"), 1920, 1080);
    const QString id = created.value(QStringLiteral("id")).toString();
    QTRY_COMPARE(repository.rowCount(), 1);
    QVERIFY(repository.rename(id, QStringLiteral("After")));
    QTRY_COMPARE(repository.data(repository.index(0), DraftRepository::NameRole).toString(),
                 QStringLiteral("After"));
    QVERIFY(repository.deleteDraft(id));
    QTRY_COMPARE(repository.rowCount(), 0);
    QVERIFY(!QDir(repository.draftDirectory(id)).exists());
}

void TestDraft::rejectClipBeyondMediaSource()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    const QVariantMap created = repository.create(QStringLiteral("Source range"), 1280, 720);
    QVERIFY(!created.isEmpty());
    const QString id = created.value(QStringLiteral("id")).toString();

    QFile draftFile(repository.draftFilePath(id));
    QVERIFY(draftFile.open(QIODevice::ReadOnly));
    QJsonObject object = QJsonDocument::fromJson(draftFile.readAll()).object();
    draftFile.close();
    object[QStringLiteral("assets")] = QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("video")},
                    {QStringLiteral("kind"), QStringLiteral("video")},
                    {QStringLiteral("name"), QStringLiteral("Video")},
                    {QStringLiteral("source"), QStringLiteral("media/video.mp4")},
                    {QStringLiteral("duration"), 1000}}
    };
    const QJsonObject clip{{QStringLiteral("id"), QStringLiteral("clip")},
                           {QStringLiteral("assetId"), QStringLiteral("video")},
                           {QStringLiteral("start"), 0},
                           {QStringLiteral("sourceIn"), 500},
                           {QStringLiteral("duration"), 600}};
    object[QStringLiteral("timeline")] = QJsonObject{
        {QStringLiteral("tracks"), QJsonArray{
            QJsonObject{{QStringLiteral("id"), QStringLiteral("V1")},
                        {QStringLiteral("kind"), QStringLiteral("main")},
                        {QStringLiteral("clips"), QJsonArray{clip}}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("A1")},
                        {QStringLiteral("kind"), QStringLiteral("audio")},
                        {QStringLiteral("clips"), QJsonArray{}}}
        }}
    };
    QVERIFY(draftFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(draftFile.write(QJsonDocument(object).toJson()) > 0);
    draftFile.close();

    AssetModel assets;
    TimelineModel timeline(&assets);
    Draft draft;
    QString error;
    QVERIFY(!repository.load(id, draft, assets, timeline, &error));
    QVERIFY(error.contains(QStringLiteral("invalid"), Qt::CaseInsensitive));
}

QTEST_GUILESS_MAIN(TestDraft)
#include "test_draft.moc"
