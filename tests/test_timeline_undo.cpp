#include "core/DraftRepository.h"
#include "editor/EditorController.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

class TestTimelineUndo : public QObject
{
    Q_OBJECT

private slots:
    void overlapMoveTrimAndProperties();
    void mergedPropertyGestureAndSaveCleanState();
    void failedOpenRestoresPreviousState();
    void savePersistsAssetsAndClips();
    void autosavePersistsChanges();
    void splitTrimAndPaste();
    void speedAdjustsClipAndTimelineDuration();
    void filterClipMoveAndTrim();
    void textStylePropertiesPersist();
};

void TestTimelineUndo::savePersistsAssetsAndClips()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Persisted"), 1280, 720));
    const QString asset = editor.assets()->addText(QStringLiteral("Overlay text"));
    const QString overlay = editor.addTrack();
    QVERIFY(!overlay.isEmpty());
    const QString clip = editor.addClip(asset, overlay, 500, 2000);
    QVERIFY(!clip.isEmpty());
    QVERIFY(editor.save());

    QFile file(editor.currentDraftPath());
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    QCOMPARE(object.value(QStringLiteral("assets")).toArray().size(), 1);
    const QJsonArray tracks = object.value(QStringLiteral("timeline"))
                                  .toObject()
                                  .value(QStringLiteral("tracks"))
                                  .toArray();
    int clipCount = 0;
    for (const QJsonValue &track : tracks)
        clipCount += track.toObject().value(QStringLiteral("clips")).toArray().size();
    QCOMPARE(clipCount, 1);
}

void TestTimelineUndo::autosavePersistsChanges()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Autosave"), 1280, 720));

    // 默认启用自动保存
    QVERIFY(editor.autosaveEnabled());

    const QString asset = editor.assets()->addText(QStringLiteral("Autosave text"));
    QVERIFY(!editor.addClip(asset, QStringLiteral("V1"), 0, 1000).isEmpty());
    QVERIFY(editor.dirty());

    // 停止编辑 3 秒后应自动写盘
    QTRY_VERIFY_WITH_TIMEOUT(!editor.dirty(), 10000);

    // 关闭自动保存后不再自动写盘
    editor.setAutosaveEnabled(false);
    QVERIFY(!editor.addClip(asset, QStringLiteral("V1"), 2000, 1000).isEmpty());
    QVERIFY(editor.dirty());
    QTest::qWait(4000);
    QVERIFY(editor.dirty());
}

void TestTimelineUndo::overlapMoveTrimAndProperties()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Timeline"), 1920, 1080));
    const QString firstAsset = editor.assets()->addText(QStringLiteral("First"));
    const QString secondAsset = editor.assets()->addText(QStringLiteral("Second"));
    const QString first = editor.addClip(firstAsset, QStringLiteral("V1"), 0, 5000);
    QVERIFY(!first.isEmpty());
    const QString autoPlaced = editor.addClip(secondAsset, QStringLiteral("V1"), 1000, 3000);
    QVERIFY(!autoPlaced.isEmpty());
    QCOMPARE(editor.timeline()->getClip(autoPlaced).value(QStringLiteral("trackId")).toString(),
             QStringLiteral("V2"));
    QVERIFY(!editor.moveClip(first, QStringLiteral("V1"), -1));

    QVERIFY(editor.moveClip(first, QStringLiteral("V1"), 6000));
    QCOMPARE(editor.timeline()->getClip(first).value(QStringLiteral("start")).toLongLong(), 6000);
    QVERIFY(editor.canUndo());
    editor.undo();
    QCOMPARE(editor.timeline()->getClip(first).value(QStringLiteral("start")).toLongLong(), 0);
    QVERIFY(editor.canRedo());
    editor.redo();
    QCOMPARE(editor.timeline()->getClip(first).value(QStringLiteral("start")).toLongLong(), 6000);

    QVERIFY(editor.trimClip(first, 6500, 250, 2500));
    QVariantMap clip = editor.timeline()->getClip(first);
    QCOMPARE(clip.value(QStringLiteral("start")).toLongLong(), 6500);
    QCOMPARE(clip.value(QStringLiteral("sourceIn")).toLongLong(), 250);
    QCOMPARE(clip.value(QStringLiteral("duration")).toLongLong(), 2500);
    editor.undo();
    clip = editor.timeline()->getClip(first);
    QCOMPARE(clip.value(QStringLiteral("start")).toLongLong(), 6000);
    QCOMPARE(clip.value(QStringLiteral("sourceIn")).toLongLong(), 0);
    QCOMPARE(clip.value(QStringLiteral("duration")).toLongLong(), 5000);
    editor.redo();

    const QString overlay = editor.addTrack();
    const QString second = editor.addClip(secondAsset, overlay, 1000, 2000);
    QVERIFY(!second.isEmpty());
    QVERIFY(!editor.moveClip(second, QStringLiteral("V1"), 7000));
    QVERIFY(editor.moveClip(second, QStringLiteral("V1"), 10000));
    QCOMPARE(editor.timeline()->getClip(second).value(QStringLiteral("trackId")).toString(),
             QStringLiteral("V1"));
    editor.undo();
    QCOMPARE(editor.timeline()->getClip(second).value(QStringLiteral("trackId")).toString(), overlay);

    QVERIFY(editor.removeClip(second));
    QVERIFY(editor.timeline()->getClip(second).isEmpty());
    editor.undo();
    QVERIFY(!editor.timeline()->getClip(second).isEmpty());
}

void TestTimelineUndo::mergedPropertyGestureAndSaveCleanState()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Properties"), 1280, 720));
    const QString asset = editor.assets()->addText(QStringLiteral("Inspect me"));
    const QString clip = editor.addClip(asset, QStringLiteral("V1"), 0, 5000);
    QVERIFY(editor.save());
    QVERIFY(!editor.dirty());

    QVERIFY(editor.setClipProperty(clip, QStringLiteral("scale"), 1.2, true));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("scale"), 1.5, true));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("scale"), 2.0, true));
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("scale")).toDouble(), 2.0);
    QVERIFY(editor.dirty());
    editor.undo();
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("scale")).toDouble(), 1.0);
    QVERIFY(!editor.dirty());
    editor.redo();
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("scale")).toDouble(), 2.0);
    QVERIFY(editor.dirty());

    QVERIFY(editor.setClipProperty(clip, QStringLiteral("opacity"), 0.5));
    QCOMPARE(editor.timeline()->selectedClipId(), clip);
    QCOMPARE(editor.timeline()->selectedClip().value(QStringLiteral("opacity")).toDouble(), 0.5);
    editor.undo();
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("opacity")).toDouble(), 1.0);
    QVERIFY(editor.save());
    QVERIFY(!editor.dirty());
}

void TestTimelineUndo::failedOpenRestoresPreviousState()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Current"), 1920, 1080));
    const QString asset = editor.assets()->addText(QStringLiteral("Keep me"));
    const QString clip = editor.addClip(asset, QStringLiteral("V1"), 0, 5000);
    QVERIFY(!clip.isEmpty());
    QVERIFY(editor.save());

    const QString previousDirectory = editor.assets()->draftDirectory();
    const QString previousPath = editor.currentDraftPath();
    const QJsonArray previousAssets = editor.assets()->toJson();
    const QJsonObject previousTimeline = editor.timeline()->toJson();

    const QVariantMap created = repository.create(QStringLiteral("Broken"), 1280, 720);
    QVERIFY(!created.isEmpty());
    const QString brokenId = created.value(QStringLiteral("id")).toString();
    QFile draftFile(repository.draftFilePath(brokenId));
    QVERIFY(draftFile.open(QIODevice::ReadOnly));
    QJsonObject broken = QJsonDocument::fromJson(draftFile.readAll()).object();
    draftFile.close();
    broken[QStringLiteral("timeline")] = QJsonObject{};
    QVERIFY(draftFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(draftFile.write(QJsonDocument(broken).toJson()) > 0);
    draftFile.close();

    QVERIFY(!editor.openDraft(brokenId));
    QCOMPARE(editor.assets()->draftDirectory(), previousDirectory);
    QCOMPARE(editor.assets()->toJson(), previousAssets);
    QCOMPARE(editor.timeline()->toJson(), previousTimeline);
    QCOMPARE(editor.timeline()->selectedClipId(), clip);
    QCOMPARE(editor.currentDraftPath(), previousPath);
    QCOMPARE(editor.currentDraftName(), QStringLiteral("Current"));
    QVERIFY(!editor.dirty());

    EditorController emptyEditor(&repository);
    QVERIFY(!emptyEditor.openDraft(brokenId));
    QVERIFY(emptyEditor.assets()->draftDirectory().isEmpty());
}

void TestTimelineUndo::splitTrimAndPaste()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Razor"), 1280, 720));
    const QString asset = editor.assets()->addText(QStringLiteral("Cut me"));
    const QString clip = editor.addClip(asset, QStringLiteral("V1"), 0, 4000);
    QVERIFY(!clip.isEmpty());

    QVERIFY(editor.splitClip(clip, 0).isEmpty());
    QVERIFY(editor.splitClip(clip, 4000).isEmpty());
    const QString right = editor.splitClip(clip, 1500);
    QVERIFY(!right.isEmpty());
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("duration")).toLongLong(), 1500);
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("start")).toLongLong(), 1500);
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("sourceIn")).toLongLong(), 1500);
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("duration")).toLongLong(), 2500);
    QCOMPARE(editor.timeline()->selectedClipId(), right);
    editor.undo();
    QVERIFY(editor.timeline()->getClip(right).isEmpty());
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("duration")).toLongLong(), 4000);
    editor.redo();

    QVERIFY(!editor.trimAt(right, 1500, true));
    QVERIFY(editor.trimAt(right, 2500, true));
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("duration")).toLongLong(), 1000);
    editor.undo();
    QVERIFY(editor.trimAt(right, 2500, false));
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("start")).toLongLong(), 2500);
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("duration")).toLongLong(), 1500);

    QVERIFY(editor.setClipPosition(clip, 120, -40));
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("x")).toDouble(), 120.0);
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("y")).toDouble(), -40.0);
    editor.undo();
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("x")).toDouble(), 0.0);

    QVERIFY(editor.setClipProperty(clip, QStringLiteral("audioDetached"), true));
    QVERIFY(editor.timeline()->getClip(clip).value(QStringLiteral("audioDetached")).toBool());
    editor.undo();
    QVERIFY(!editor.timeline()->getClip(clip).value(QStringLiteral("audioDetached")).toBool());

    const QVariantMap copied = editor.timeline()->getClip(clip);
    const QString pasted = editor.insertClip(copied, QStringLiteral("V1"), 5000);
    QVERIFY(!pasted.isEmpty());
    QCOMPARE(editor.timeline()->getClip(pasted).value(QStringLiteral("start")).toLongLong(), 5000);
    QCOMPARE(editor.timeline()->selectedClipId(), pasted);
    editor.undo();
    QVERIFY(editor.timeline()->getClip(pasted).isEmpty());

    QVERIFY(editor.assets()->contains(QStringLiteral("preset-black")));
    QCOMPARE(editor.assets()->kind(QStringLiteral("preset-white")), QStringLiteral("color"));
    const QString black = editor.addClip(QStringLiteral("preset-black"), QStringLiteral("V1"), 12000);
    QVERIFY(!black.isEmpty());
    QCOMPARE(editor.timeline()->getClip(black).value(QStringLiteral("duration")).toLongLong(), 5000);
}

void TestTimelineUndo::speedAdjustsClipAndTimelineDuration()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Speed"), 1280, 720));
    const QString asset = editor.assets()->addText(QStringLiteral("Variable speed"));
    const QString clip = editor.addClip(asset, QStringLiteral("V1"), 0, 4000);
    QVERIFY(!clip.isEmpty());
    QCOMPARE(editor.timeline()->duration(), 4000);

    QVERIFY(editor.setClipProperty(clip, QStringLiteral("speed"), 2.0));
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("duration")).toLongLong(), 2000);
    QCOMPARE(editor.timeline()->duration(), 2000);
    editor.undo();
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("duration")).toLongLong(), 4000);
    QCOMPARE(editor.timeline()->duration(), 4000);
    editor.redo();
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("duration")).toLongLong(), 2000);

    const QString right = editor.splitClip(clip, 500);
    QVERIFY(!right.isEmpty());
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("sourceIn")).toLongLong(), 1000);
    QCOMPARE(editor.timeline()->getClip(right).value(QStringLiteral("duration")).toLongLong(), 1500);
    editor.undo();

    QVERIFY(editor.setClipProperty(clip, QStringLiteral("speed"), 0.5));
    QCOMPARE(editor.timeline()->getClip(clip).value(QStringLiteral("duration")).toLongLong(), 8000);
    QCOMPARE(editor.timeline()->duration(), 8000);
}

void TestTimelineUndo::filterClipMoveAndTrim()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("Filter"), 1280, 720));

    const QString filterTrack = editor.addFilterTrack();
    QVERIFY(!filterTrack.isEmpty());
    const QString filter = editor.addClip(QStringLiteral("preset-filter-grayscale"),
                                         filterTrack, 0, 5000);
    QVERIFY(!filter.isEmpty());
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("duration")).toLongLong(), 5000);

    // 同轨内平移
    QVERIFY(editor.moveClip(filter, filterTrack, 2000));
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("start")).toLongLong(), 2000);

    // 拉伸时长（右侧）
    QVERIFY(editor.trimClip(filter, 2000, 0, 9000));
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("duration")).toLongLong(), 9000);

    // 左侧裁剪
    QVERIFY(editor.trimClip(filter, 3000, 1000, 8000));
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("start")).toLongLong(), 3000);
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("sourceIn")).toLongLong(), 1000);
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("duration")).toLongLong(), 8000);

    // 拖到视频轨应被拒绝
    QVERIFY(!editor.moveClip(filter, QStringLiteral("V1"), 0));

    // 平移到无冲突位置应成功
    QVERIFY(editor.moveClip(filter, filterTrack, 20000));
    QCOMPARE(editor.timeline()->getClip(filter).value(QStringLiteral("start")).toLongLong(), 20000);
}

void TestTimelineUndo::textStylePropertiesPersist()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("TextStyle"), 1280, 720));

    const QString asset = editor.assets()->addText(QStringLiteral("Styled"));
    const QString track = editor.addTrack();
    QVERIFY(!track.isEmpty());
    const QString clip = editor.addClip(asset, track, 0, 3000);
    QVERIFY(!clip.isEmpty());

    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textFont"), QStringLiteral("DejaVu Sans")));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textBold"), true));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textItalic"), true));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textUnderline"), true));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textStrikeout"), true));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textFillColor"), QStringLiteral("#ff0000")));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textStrokeColor"), QStringLiteral("#000000")));
    QVERIFY(editor.setClipProperty(clip, QStringLiteral("textShadowColor"), QStringLiteral("#80202020")));

    const QVariantMap current = editor.timeline()->getClip(clip);
    QCOMPARE(current.value(QStringLiteral("textFont")).toString(), QStringLiteral("DejaVu Sans"));
    QCOMPARE(current.value(QStringLiteral("textBold")).toBool(), true);
    QCOMPARE(current.value(QStringLiteral("textShadowColor")).toString(), QStringLiteral("#80202020"));

    QVERIFY(editor.save());
    QFile file(editor.currentDraftPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QJsonObject savedClip;
    const QJsonArray tracks = object.value(QStringLiteral("timeline"))
                                  .toObject()
                                  .value(QStringLiteral("tracks"))
                                  .toArray();
    for (const QJsonValue &trackValue : tracks) {
        const QJsonArray clips = trackValue.toObject().value(QStringLiteral("clips")).toArray();
        for (const QJsonValue &clipValue : clips) {
            if (clipValue.toObject().value(QStringLiteral("id")).toString() == clip) {
                savedClip = clipValue.toObject();
                break;
            }
        }
    }
    QVERIFY(!savedClip.isEmpty());
    QCOMPARE(savedClip.value(QStringLiteral("textUnderline")).toBool(), true);
    QCOMPARE(savedClip.value(QStringLiteral("textFillColor")).toString(), QStringLiteral("#ff0000"));
    QCOMPARE(savedClip.value(QStringLiteral("textShadowColor")).toString(), QStringLiteral("#80202020"));
}

QTEST_GUILESS_MAIN(TestTimelineUndo)
#include "test_timeline_undo.moc"
