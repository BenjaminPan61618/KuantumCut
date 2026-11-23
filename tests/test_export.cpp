#include "core/DraftRepository.h"
#include "editor/EditorController.h"
#include "editor/ExportController.h"

#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class TestExport : public QObject
{
    Q_OBJECT

private slots:
    void exportsTimeline();
};

void TestExport::exportsTimeline()
{
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        QSKIP("系统没有 ffmpeg，跳过导出测试");

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString source = temporary.filePath(QStringLiteral("source.mp4"));
    QProcess generate;
    generate.start(ffmpeg, {QStringLiteral("-y"), QStringLiteral("-hide_banner"),
                            QStringLiteral("-loglevel"), QStringLiteral("error"),
                            QStringLiteral("-f"), QStringLiteral("lavfi"),
                            QStringLiteral("-i"), QStringLiteral("testsrc=size=320x240:rate=30:duration=2"),
                            QStringLiteral("-f"), QStringLiteral("lavfi"),
                            QStringLiteral("-i"), QStringLiteral("sine=frequency=440:duration=2"),
                            QStringLiteral("-c:v"), QStringLiteral("mpeg4"),
                            QStringLiteral("-q:v"), QStringLiteral("5"),
                            QStringLiteral("-c:a"), QStringLiteral("aac"),
                            QStringLiteral("-shortest"), source});
    QVERIFY(generate.waitForFinished(60000));
    QCOMPARE(generate.exitCode(), 0);
    QVERIFY(QFileInfo(source).size() > 0);

    DraftRepository repository(temporary.filePath(QStringLiteral("library")));
    EditorController editor(&repository);
    QVERIFY(editor.newDraft(QStringLiteral("ExportTest"), 640, 360));
    const QString asset = editor.assets()->importMedia(QUrl::fromLocalFile(source));
    QVERIFY(!asset.isEmpty());
    QCOMPARE(editor.assets()->kind(asset), QStringLiteral("video"));
    QVERIFY(!editor.addClip(asset, QStringLiteral("V1"), 0, 1000).isEmpty());

    const QString imgPath = temporary.filePath(QStringLiteral("img.png"));
    QImage img(320, 240, QImage::Format_RGB32);
    img.fill(Qt::red);
    img.save(imgPath);
    const QString imgAsset = editor.assets()->importMedia(QUrl::fromLocalFile(imgPath));
    QVERIFY(!imgAsset.isEmpty());
    QVERIFY(!editor.addClip(imgAsset, QStringLiteral("V1"), 1000, 2000).isEmpty());

    // 变速 + 非零起始位置：曾因 ffmpeg 中 atempo 后接 adelay 产生非法音频时间戳，
    // 导致导出的音频流起始时间溢出、无法播放。
    const QString speedClip = editor.addClip(asset, QStringLiteral("V1"), 3000, 800);
    QVERIFY(!speedClip.isEmpty());
    QVERIFY(editor.setClipProperty(speedClip, QStringLiteral("speed"), 2.0));

    const QString filterTrack = editor.addFilterTrack();
    QVERIFY(!filterTrack.isEmpty());
    QVERIFY(!editor.addClip(QStringLiteral("preset-filter-grayscale"), filterTrack, 0, 500).isEmpty());
    QVERIFY(!editor.addClip(QStringLiteral("preset-filter-keep-red"), filterTrack, 500, 500).isEmpty());

    ExportController exporter(&editor);
    exporter.setFrameRate(25);
    exporter.setVideoBitrate(1200);
    exporter.setAudioBitrate(96);
    QSignalSpy finished(&exporter, &ExportController::finished);
    const QString output = temporary.filePath(QStringLiteral("out.mp4"));
    QVERIFY(exporter.start(output));
    QVERIFY(finished.wait(90000));
    const QList<QVariant> result = finished.takeFirst();
    QVERIFY2(result.at(0).toBool(), qPrintable(exporter.error()));
    QVERIFY(QFileInfo(output).size() > 0);

    const QString ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (!ffprobe.isEmpty()) {
        QProcess probe;
        probe.start(ffprobe, {QStringLiteral("-v"), QStringLiteral("error"),
                              QStringLiteral("-select_streams"), QStringLiteral("v:0"),
                              QStringLiteral("-show_entries"), QStringLiteral("stream=avg_frame_rate"),
                              QStringLiteral("-of"), QStringLiteral("csv=p=0"), output});
        QVERIFY(probe.waitForFinished(15000));
        QCOMPARE(QString::fromUtf8(probe.readAllStandardOutput()).trimmed(),
                 QStringLiteral("25/1"));

        // 音频流必须存在，且时间戳正常（起始接近 0、时长与时间线一致）
        QProcess audioProbe;
        audioProbe.start(ffprobe, {QStringLiteral("-v"), QStringLiteral("error"),
                                   QStringLiteral("-select_streams"), QStringLiteral("a:0"),
                                   QStringLiteral("-show_entries"),
                                   QStringLiteral("stream=codec_name,start_time,duration"),
                                   QStringLiteral("-of"), QStringLiteral("csv=p=0"), output});
        QVERIFY(audioProbe.waitForFinished(15000));
        const QStringList fields = QString::fromUtf8(audioProbe.readAllStandardOutput())
                                       .trimmed().split(QLatin1Char(','));
        QVERIFY2(fields.size() >= 3, "导出文件缺少音频流");
        QCOMPARE(fields.at(0), QStringLiteral("aac"));
        const double audioStart = fields.at(1).toDouble();
        const double audioDuration = fields.at(2).toDouble();
        QVERIFY2(audioStart >= 0.0 && audioStart < 1.0,
                 qPrintable(QStringLiteral("音频起始时间异常: %1").arg(audioStart)));
        QVERIFY2(audioDuration > 2.5 && audioDuration < 5.0,
                 qPrintable(QStringLiteral("音频时长异常: %1").arg(audioDuration)));
    }
}

QTEST_MAIN(TestExport)
#include "test_export.moc"
