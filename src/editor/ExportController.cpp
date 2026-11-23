#include "editor/ExportController.h"

#include "editor/EditorController.h"
#include "models/AssetModel.h"
#include "models/TimelineModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QDesktopServices>
#include <QUuid>
#include <algorithm>

namespace {

QString secondsText(qint64 milliseconds)
{
    return QString::number(milliseconds / 1000.0, 'f', 3);
}

// 优先使用系统 ffmpeg/ffprobe，避免命中 IDE/沙箱自带的精简版
// （例如 TRAE 自带的 ffmpeg 缺少 overlay/amix/image2/lavfi 等，无法完成合成）
QString systemExecutable(const QString &name)
{
    const QStringList candidates = {
        QStringLiteral("/usr/bin/") + name,
        QStringLiteral("/usr/local/bin/") + name,
        QStringLiteral("/bin/") + name
    };
    for (const QString &path : candidates)
        if (QFileInfo::exists(path))
            return path;
    return QStandardPaths::findExecutable(name);
}

// libx264 优先；系统 ffmpeg 没编进 x264 时回退到通用 mpeg4
QString videoEncoder()
{
    static QString cached;
    if (!cached.isEmpty())
        return cached;
    const QString ffmpeg = systemExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return QString();
    QProcess process;
    process.start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
    if (!process.waitForFinished(5000)) {
        cached = QStringLiteral("mpeg4");
        return cached;
    }
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (output.contains(QStringLiteral(" libx264 ")))
        cached = QStringLiteral("libx264");
    else if (output.contains(QStringLiteral(" libopenh264 ")))
        cached = QStringLiteral("libopenh264");
    else
        cached = QStringLiteral("mpeg4");
    return cached;
}

} // namespace

ExportController::ExportController(EditorController *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
{
}

void ExportController::setFrameRate(int value)
{
    value = qBound(1, value, 120);
    if (m_frameRate == value)
        return;
    m_frameRate = value;
    emit frameRateChanged();
}

void ExportController::setVideoBitrate(int value)
{
    value = qBound(100, value, 200000);
    if (m_videoBitrate == value)
        return;
    m_videoBitrate = value;
    emit videoBitrateChanged();
}

void ExportController::setAudioBitrate(int value)
{
    value = qBound(32, value, 512);
    if (m_audioBitrate == value)
        return;
    m_audioBitrate = value;
    emit audioBitrateChanged();
}

QString ExportController::suggestFileName() const
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (directory.isEmpty())
        directory = QDir::homePath();
    QString name = m_editor->currentDraftName().trimmed();
    if (name.isEmpty())
        name = QStringLiteral("export");
    return QDir(directory).absoluteFilePath(name + QStringLiteral(".mp4"));
}

void ExportController::setError(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}

void ExportController::setProgress(double value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_progress, value))
        return;
    m_progress = value;
    emit progressChanged();
}

bool ExportController::start(const QString &outputPath, bool overwrite)
{
    if (m_running)
        return false;
    QString target = outputPath.trimmed();
    if (target.startsWith(QStringLiteral("file://")))
        target = target.mid(7);
    if (target.isEmpty()) {
        setError(tr("请选择输出文件。"));
        return false;
    }
    // 清除上一次导出结果
    if (!m_outputPath.isEmpty()) {
        m_outputPath.clear();
        emit outputPathChanged();
    }

    const QFileInfo info(target);
    const QString suffix = info.suffix();
    static const QStringList supported = {QStringLiteral("mp4"), QStringLiteral("mov"),
                                          QStringLiteral("avi"), QStringLiteral("wmv")};
    if (suffix.isEmpty()) {
        target += QStringLiteral(".mp4");
    } else if (!supported.contains(suffix, Qt::CaseInsensitive)) {
        setError(tr("仅支持 .mp4 / .mov / .avi / .wmv 输出格式，请更换文件扩展名。"));
        return false;
    }

    // 检查文件名是否合规（不含路径分隔符、控制字符等）
    const QString fileName = QFileInfo(target).fileName();
    if (fileName.isEmpty() || fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
        setError(tr("文件名无效，请重新输入。"));
        return false;
    }
    for (const QChar &ch : fileName) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch.isNull() || ch < QLatin1Char(' ')) {
            setError(tr("文件名含有非法字符，请重新输入。"));
            return false;
        }
    }

    if (QFileInfo::exists(target)) {
        if (!overwrite) {
            emit overwriteConfirmationRequested(target);
            return false;
        }
    }

    const QString ffmpeg = systemExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        setError(tr("未找到 ffmpeg，请先安装后再导出。"));
        return false;
    }

    QStringList arguments;
    QString buildError;
    if (!buildArguments(target, &arguments, &buildError)) {
        setError(buildError);
        return false;
    }

    QDir().mkpath(QFileInfo(target).absolutePath());
    m_target = target;
    m_stderr.clear();
    m_cancelled = false;
    setError(QString());
    setProgress(0.0);
    m_running = true;
    emit runningChanged();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() { parseProgress(); });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_stderr += m_process->readAllStandardError();
        if (m_stderr.size() > 8192)
            m_stderr = m_stderr.right(8192);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            handleFinished(-1, QProcess::CrashExit);
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) { handleFinished(exitCode, status); });
    m_process->start(ffmpeg, arguments);
    return true;
}

void ExportController::cancel()
{
    if (!m_running || !m_process)
        return;
    m_cancelled = true;
    m_process->kill();
}

void ExportController::clearResult()
{
    if (m_running)
        return;
    if (!m_outputPath.isEmpty()) {
        m_outputPath.clear();
        emit outputPathChanged();
    }
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }
    if (!qFuzzyIsNull(m_progress)) {
        m_progress = 0.0;
        emit progressChanged();
    }
}

void ExportController::openOutputFile()
{
    if (m_outputPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputPath));
}

void ExportController::openOutputFolder()
{
    if (m_outputPath.isEmpty())
        return;
    const QString dir = QFileInfo(m_outputPath).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void ExportController::parseProgress()
{
    if (!m_process)
        return;
    const QList<QByteArray> lines = m_process->readAllStandardOutput().split('\n');
    for (const QByteArray &line : lines) {
        if (!line.startsWith("out_time_us=") && !line.startsWith("out_time_ms="))
            continue;
        const qint64 microseconds = line.mid(line.indexOf('=') + 1).trimmed().toLongLong();
        if (m_durationMs > 0)
            setProgress(double(microseconds) / (double(m_durationMs) * 1000.0));
    }
}

void ExportController::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_running)
        return;
    m_running = false;
    emit runningChanged();
    const bool success = !m_cancelled && status == QProcess::NormalExit && exitCode == 0;
    if (success) {
        setProgress(1.0);
        m_outputPath = m_target;
        emit outputPathChanged();
        setError(QString());
    } else if (m_cancelled) {
        setError(tr("导出已取消。"));
    } else {
        QString text = QString::fromUtf8(m_stderr).trimmed();
        if (text.size() > 600)
            text = text.right(600);
        setError(text.isEmpty() ? tr("导出失败（ffmpeg 退出码 %1）。").arg(exitCode) : text);
    }
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    for (const QString &file : m_tempFiles)
        QFile::remove(file);
    m_tempFiles.clear();
    emit finished(success);
}

QString ExportController::renderTextImage(const QString &text, int width, int height,
                                          const TimelineClip &clip)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font = painter.font();
    if (!clip.textFont.isEmpty())
        font.setFamily(clip.textFont);
    font.setPixelSize(qMax(18, int(height * 0.07)));
    font.setBold(clip.textBold);
    font.setItalic(clip.textItalic);
    font.setUnderline(clip.textUnderline);
    font.setStrikeOut(clip.textStrikeout);
    painter.setFont(font);
    const qreal margin = width * 0.06;
    const QRectF rect(margin, margin, width - 2 * margin, height - 2 * margin);
    const int flags = Qt::AlignCenter | Qt::TextWordWrap;

    const QColor fillColor(clip.textFillColor);
    const QColor strokeColor(clip.textStrokeColor);
    const QColor shadowColor(clip.textShadowColor);

    // 阴影：整体偏移后先绘制
    if (shadowColor.isValid() && shadowColor.alpha() > 0) {
        const int offset = qMax(2, font.pixelSize() / 20);
        painter.setPen(shadowColor);
        painter.drawText(rect.translated(offset, offset), flags, text);
    }
    // 描边：沿圆形范围多次偏移绘制
    if (strokeColor.isValid() && strokeColor.alpha() > 0) {
        const int strokeWidth = qMax(1, font.pixelSize() / 24);
        painter.setPen(strokeColor);
        for (int dx = -strokeWidth; dx <= strokeWidth; ++dx) {
            for (int dy = -strokeWidth; dy <= strokeWidth; ++dy) {
                if (dx == 0 && dy == 0)
                    continue;
                if (dx * dx + dy * dy > strokeWidth * strokeWidth)
                    continue;
                painter.drawText(rect.translated(dx, dy), flags, text);
            }
        }
    }
    painter.setPen(fillColor.isValid() ? fillColor : QColor(Qt::white));
    painter.drawText(rect, flags, text);
    painter.end();

    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                             .filePath(QStringLiteral("kuantumcut_text_%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (image.save(path, "PNG")) {
        m_tempFiles << path;
        return path;
    }
    return QString();
}

bool ExportController::buildArguments(const QString &target, QStringList *arguments, QString *error)
{
    m_tempFiles.clear();
    TimelineModel *model = m_editor->timeline();
    AssetModel *assets = m_editor->assets();
    const qint64 duration = model->duration();
    if (duration <= 0) {
        *error = tr("时间线为空，无法导出。");
        return false;
    }
    int width = m_editor->canvasWidth();
    int height = m_editor->canvasHeight();
    width -= width % 2;
    height -= height % 2;
    if (width <= 0 || height <= 0) {
        *error = tr("画布尺寸无效，无法导出。");
        return false;
    }
    const QString encoder = videoEncoder();
    if (encoder.isEmpty()) {
        *error = tr("未找到可用的 ffmpeg 编码器。");
        return false;
    }

    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                     QStringLiteral("error"), QStringLiteral("-nostdin"),
                     QStringLiteral("-progress"), QStringLiteral("pipe:1"),
                     QStringLiteral("-nostats"), QStringLiteral("-y")};
    QStringList graph;
    QStringList audioLabels;
    int inputIndex = 0;
    int visualIndex = 0;
    int audioIndex = 0;

    graph << QStringLiteral("color=c=black:s=%1x%2:r=%3:d=%4,format=rgba[base]")
                 .arg(width)
                 .arg(height)
                 .arg(m_frameRate)
                 .arg(secondsText(duration));
    QString visualLabel = QStringLiteral("base");
    m_durationMs = duration;

    QHash<QString, bool> audioStreams;
    auto hasAudioStream = [&audioStreams](const QString &path) {
        const auto cached = audioStreams.constFind(path);
        if (cached != audioStreams.constEnd())
            return cached.value();
        QProcess probe;
        probe.start(systemExecutable(QStringLiteral("ffprobe")),
                    {QStringLiteral("-v"), QStringLiteral("error"),
                     QStringLiteral("-select_streams"), QStringLiteral("a"),
                     QStringLiteral("-show_entries"), QStringLiteral("stream=index"),
                     QStringLiteral("-of"), QStringLiteral("csv=p=0"), path});
        const bool result = probe.waitForFinished(5000)
                            && !probe.readAllStandardOutput().trimmed().isEmpty();
        audioStreams.insert(path, result);
        return result;
    };
    auto atempoFilters = [](double speed) {
        // atempo 仅支持 0.5~2.0，超出范围时串联多个 atempo
        QStringList filters;
        double remaining = speed;
        while (remaining > 2.0) {
            filters << QStringLiteral("atempo=2.0");
            remaining /= 2.0;
        }
        while (remaining < 0.5) {
            filters << QStringLiteral("atempo=0.5");
            remaining /= 0.5;
        }
        filters << QStringLiteral("atempo=%1").arg(remaining, 0, 'f', 5);
        return filters.join(QLatin1Char(','));
    };

    auto appendAudio = [&graph, &audioLabels, &audioIndex, &atempoFilters](int index, const TimelineClip &clip) {
        const QString label = QStringLiteral("a%1").arg(++audioIndex);
        const double speed = qMax(0.01, clip.speed);
        QString chain = QStringLiteral("[%1:a]asetpts=PTS-STARTPTS").arg(index);
        if (clip.reverse && clip.originalAssetId.isEmpty())
            chain += QStringLiteral(",areverse");
        // adelay 必须放在 atempo 之前：当前 ffmpeg 中 atempo 后接 adelay 会输出
        // AV_NOPTS_VALUE 时间戳，经 amix 混音后导致音频流起始时间溢出（无法播放、
        // 部分播放器还会把总时长显示成十几万小时）。先按“延迟 × 速度”插入静音，
        // 再由 atempo 变速还原为实际延迟。
        if (clip.start > 0)
            chain += QStringLiteral(",adelay=%1:all=1").arg(qRound64(clip.start * speed));
        if (!qFuzzyCompare(speed, 1.0))
            chain += QStringLiteral(",") + atempoFilters(speed);
        if (clip.volume < 0.999)
            chain += QStringLiteral(",volume=%1").arg(clip.volume, 0, 'f', 3);
        chain += QStringLiteral("[%1]").arg(label);
        graph << chain;
        audioLabels << QStringLiteral("[%1]").arg(label);
    };

    const QList<TimelineTrack> tracks = model->tracks();
    for (const TimelineTrack &track : tracks) {
        const bool audioTrack = track.kind == QStringLiteral("audio");
        for (const TimelineClip &clip : track.clips) {
            if (clip.duration <= 0)
                continue;
            const QString kind = assets->kind(clip.assetId);
            if (kind != QStringLiteral("text") && kind != QStringLiteral("color")
                && assets->missing(clip.assetId)) {
                *error = tr("素材文件缺失：%1").arg(assets->name(clip.assetId));
                return false;
            }
            const QString path = assets->absoluteSource(clip.assetId);
            const bool silent = clip.audioDetached || track.muted || clip.volume < 0.001;
            const bool wantsAudio = (kind == QStringLiteral("audio")
                                     || kind == QStringLiteral("video"))
                                    && !silent && hasAudioStream(path);

            if (audioTrack) {
                if (kind != QStringLiteral("audio") && kind != QStringLiteral("video"))
                    continue;
                if (!wantsAudio)
                    continue;
                const qint64 sourceDuration = qint64(clip.duration * qMax(0.01, clip.speed));
                args << QStringLiteral("-ss") << secondsText(clip.sourceIn)
                     << QStringLiteral("-t") << secondsText(sourceDuration)
                     << QStringLiteral("-i") << path;
                appendAudio(inputIndex++, clip);
                continue;
            }

            if (kind == QStringLiteral("text")) {
                const QString textPath = renderTextImage(assets->text(clip.assetId), width, height,
                                                         clip);
                if (textPath.isEmpty()) {
                    *error = tr("文字图层渲染失败：%1").arg(assets->name(clip.assetId));
                    return false;
                }
                args << QStringLiteral("-loop") << QStringLiteral("1")
                     << QStringLiteral("-framerate") << QString::number(m_frameRate)
                     << QStringLiteral("-t") << secondsText(clip.duration)
                     << QStringLiteral("-i") << textPath;
            } else if (kind == QStringLiteral("video")) {
                const qint64 sourceDuration = qint64(clip.duration * qMax(0.01, clip.speed));
                args << QStringLiteral("-ss") << secondsText(clip.sourceIn)
                     << QStringLiteral("-t") << secondsText(sourceDuration)
                     << QStringLiteral("-i") << path;
            } else if (kind == QStringLiteral("image")) {
                args << QStringLiteral("-loop") << QStringLiteral("1")
                     << QStringLiteral("-framerate") << QString::number(m_frameRate)
                     << QStringLiteral("-t") << secondsText(clip.duration)
                     << QStringLiteral("-i") << path;
            } else if (kind == QStringLiteral("color")) {
                args << QStringLiteral("-f") << QStringLiteral("lavfi")
                     << QStringLiteral("-t") << secondsText(clip.duration)
                     << QStringLiteral("-i")
                     << QStringLiteral("color=c=%1:s=%2x%3:r=%4")
                            .arg(assets->color(clip.assetId).isEmpty()
                                     ? QStringLiteral("#000000")
                                     : assets->color(clip.assetId))
                            .arg(width)
                            .arg(height)
                            .arg(m_frameRate);
            } else {
                continue;
            }
            const int index = inputIndex++;
            const QString label = QStringLiteral("v%1").arg(++visualIndex);
            QStringList chainParts;
            chainParts << QStringLiteral("[%1:v]format=rgba").arg(index);
            if (kind != QStringLiteral("color") && kind != QStringLiteral("text")) {
                chainParts << QStringLiteral("scale=%1:%2:force_original_aspect_ratio=decrease")
                                  .arg(width)
                                  .arg(height);
                chainParts << QStringLiteral("pad=%1:%2:(ow-iw)/2:(oh-ih)/2:color=black@0")
                                  .arg(width)
                                  .arg(height);
            }
            if (clip.flipHorizontal)
                chainParts << QStringLiteral("hflip");
            if (clip.flipVertical)
                chainParts << QStringLiteral("vflip");
            const double left = clip.cropLeft;
            const double top = clip.cropTop;
            const double right = clip.cropRight;
            const double bottom = clip.cropBottom;
            if (left + top + right + bottom > 0.0001) {
                chainParts << QStringLiteral("crop=w=iw*%1:h=ih*%2:x=iw*%3:y=ih*%4")
                                  .arg(1.0 - left - right, 0, 'f', 6)
                                  .arg(1.0 - top - bottom, 0, 'f', 6)
                                  .arg(left, 0, 'f', 6)
                                  .arg(top, 0, 'f', 6);
                chainParts << QStringLiteral("pad=%1:%2:%3:%4:color=black@0")
                                  .arg(width)
                                  .arg(height)
                                  .arg(width * left, 0, 'f', 0)
                                  .arg(height * top, 0, 'f', 0);
            }
            const double clipScale = qMax(0.01, clip.scale);
            if (!qFuzzyCompare(clipScale, 1.0)) {
                chainParts << QStringLiteral("scale=iw*%1:ih*%1:flags=bilinear")
                                  .arg(clipScale, 0, 'f', 5);
            }
            if (qAbs(clip.rotation) > 0.001) {
                // Qt 旋转为顺时针，ffmpeg rotate 为逆时针，故取反
                chainParts << QStringLiteral("rotate=%1*PI/180:ow=rotw(iw):oh=roth(ih):c=0x00000000")
                                  .arg(-clip.rotation, 0, 'f', 5);
            }
            if (clip.opacity < 0.999) {
                chainParts << QStringLiteral("colorchannelmixer=aa=%1").arg(clip.opacity, 0, 'f', 5);
            }
            // 倒放：若素材已预渲染为倒放版本，则不再重复应用 reverse 滤镜
            if (clip.reverse && kind == QStringLiteral("video") && clip.originalAssetId.isEmpty())
                chainParts << QStringLiteral("reverse");
            const double speed = qMax(0.01, clip.speed);
            if (kind == QStringLiteral("video")) {
                chainParts << QStringLiteral("setpts=(PTS-STARTPTS)/%1+%2/TB[%3]")
                                  .arg(speed, 0, 'f', 5)
                                  .arg(secondsText(clip.start), label);
            } else {
                chainParts << QStringLiteral("setpts=PTS-STARTPTS+%1/TB[%2]")
                                  .arg(secondsText(clip.start), label);
            }
            graph << chainParts.join(QLatin1Char(','));
            const QString outLabel = QStringLiteral("o%1").arg(visualIndex);
            // 图层中心对齐到画布坐标 (clip.x + W/2, clip.y + H/2)
            graph << QStringLiteral("[%1][%2]overlay=x=%3+%4/2-overlay_w/2:y=%5+%6/2-overlay_h/2:"
                                    "eof_action=pass:repeatlast=0:enable='between(t,%7,%8)'[%9]")
                         .arg(visualLabel, label,
                              QString::number(clip.x, 'f', 2), QString::number(width),
                              QString::number(clip.y, 'f', 2), QString::number(height),
                              secondsText(clip.start), secondsText(clip.start + clip.duration),
                              outLabel);
            visualLabel = outLabel;

            if (wantsAudio) {
                const qint64 sourceDuration = qint64(clip.duration * qMax(0.01, clip.speed));
                args << QStringLiteral("-ss") << secondsText(clip.sourceIn)
                     << QStringLiteral("-t") << secondsText(sourceDuration)
                     << QStringLiteral("-i") << path;
                appendAudio(inputIndex++, clip);
            }
        }
    }

    // 滤镜叠加轨道：对合成后的画面按时间段应用颜色滤镜
    {
        QList<TimelineClip> filterClips;
        for (const TimelineTrack &track : tracks) {
            if (track.kind != QStringLiteral("filter") || track.muted)
                continue;
            for (const TimelineClip &clip : track.clips)
                filterClips.append(clip);
        }
        std::sort(filterClips.begin(), filterClips.end(),
                  [&model](const TimelineClip &a, const TimelineClip &b) {
                      return model->trackZ(a.trackId) < model->trackZ(b.trackId);
                  });
        int filterStage = 0;
        for (const TimelineClip &clip : filterClips) {
            const QString filterFg = TimelineModel::colorFilterFfmpeg(clip.colorFilter,
                                                                      clip.filterStrength);
            if (filterFg.isEmpty())
                continue;
            const QString enable = QStringLiteral("enable='between(t,%1,%2)'")
                                       .arg(secondsText(clip.start))
                                       .arg(secondsText(clip.start + clip.duration));
            QStringList parts = filterFg.split(QLatin1Char(','));
            for (QString &part : parts)
                part += QLatin1Char(':') + enable;
            const QString outLabel = QStringLiteral("f%1").arg(++filterStage);
            graph << QStringLiteral("[%1]%2[%3]").arg(visualLabel, parts.join(QLatin1Char(',')),
                                                      outLabel);
            visualLabel = outLabel;
        }
    }

    graph << QStringLiteral("[%1]format=yuv420p[vout]").arg(visualLabel);
    if (!audioLabels.isEmpty()) {
        graph << audioLabels.join(QString()) + QStringLiteral("amix=inputs=%1:duration=longest:normalize=0[aout]")
                                        .arg(audioLabels.size());
    }

    const QString suffix = QFileInfo(target).suffix().toLower();
    QString videoCodec = encoder;
    QString audioCodec = QStringLiteral("aac");
    if (suffix == QStringLiteral("avi")) {
        videoCodec = QStringLiteral("mpeg4");
        audioCodec = QStringLiteral("libmp3lame");
    } else if (suffix == QStringLiteral("wmv")) {
        videoCodec = QStringLiteral("wmv2");
        audioCodec = QStringLiteral("wmav2");
    }

    *arguments = args;
    *arguments << QStringLiteral("-filter_complex")
               << graph.join(QLatin1Char(';'))
               << QStringLiteral("-map") << QStringLiteral("[vout]");
    if (!audioLabels.isEmpty())
        *arguments << QStringLiteral("-map") << QStringLiteral("[aout]");
    *arguments << QStringLiteral("-c:v") << videoCodec;
    if (videoCodec == QStringLiteral("libx264"))
        *arguments << QStringLiteral("-preset") << QStringLiteral("veryfast");
    *arguments << QStringLiteral("-b:v") << QStringLiteral("%1k").arg(m_videoBitrate)
               << QStringLiteral("-r") << QString::number(m_frameRate)
               << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    if (!audioLabels.isEmpty())
        *arguments << QStringLiteral("-c:a") << audioCodec
                   << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(m_audioBitrate);
    if (suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mov"))
        *arguments << QStringLiteral("-movflags") << QStringLiteral("+faststart");
    *arguments << QStringLiteral("-t") << secondsText(duration)
               << target;
    return true;
}
