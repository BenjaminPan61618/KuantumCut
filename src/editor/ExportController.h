#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>

class EditorController;
struct TimelineClip;

// 使用系统 ffmpeg 将当前时间线合成为视频文件
class ExportController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(int frameRate READ frameRate WRITE setFrameRate NOTIFY frameRateChanged)
    Q_PROPERTY(int videoBitrate READ videoBitrate WRITE setVideoBitrate NOTIFY videoBitrateChanged)
    Q_PROPERTY(int audioBitrate READ audioBitrate WRITE setAudioBitrate NOTIFY audioBitrateChanged)

public:
    explicit ExportController(EditorController *editor, QObject *parent = nullptr);

    bool running() const { return m_running; }
    double progress() const { return m_progress; }
    QString error() const { return m_error; }
    QString outputPath() const { return m_outputPath; }
    int frameRate() const { return m_frameRate; }
    int videoBitrate() const { return m_videoBitrate; }
    int audioBitrate() const { return m_audioBitrate; }

    void setFrameRate(int value);
    void setVideoBitrate(int value);
    void setAudioBitrate(int value);

    Q_INVOKABLE QString suggestFileName() const;
    Q_INVOKABLE bool start(const QString &outputPath, bool overwrite = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearResult();
    Q_INVOKABLE void openOutputFile();
    Q_INVOKABLE void openOutputFolder();

signals:
    void runningChanged();
    void progressChanged();
    void errorChanged();
    void outputPathChanged();
    void frameRateChanged();
    void videoBitrateChanged();
    void audioBitrateChanged();
    void finished(bool success);
    void overwriteConfirmationRequested(const QString &path);

private:
    void setError(const QString &message);
    void setProgress(double value);
    void parseProgress();
    void handleFinished(int exitCode, QProcess::ExitStatus status);
    bool buildArguments(const QString &target, QStringList *arguments, QString *error);
    QString renderTextImage(const QString &text, int width, int height,
                            const TimelineClip &clip);

    EditorController *m_editor;
    QProcess *m_process = nullptr;
    QByteArray m_stderr;
    QString m_outputPath;
    QString m_error;
    QString m_target;
    QStringList m_tempFiles;
    double m_progress = 0.0;
    qint64 m_durationMs = 0;
    int m_frameRate = 30;
    int m_videoBitrate = 8000;
    int m_audioBitrate = 192;
    bool m_running = false;
    bool m_cancelled = false;
};
