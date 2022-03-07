#ifndef DOWNLOADPROCESS_H
#define DOWNLOADPROCESS_H

#include <QObject>
#include <QProcess>
#include <QDebug>
#include <QSettings>

class DownloadProcess : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProcessState state READ getState WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(QMap<QString,QString> downloadProgress READ getDownloadProgress WRITE setDownloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(Status status READ getStatus WRITE setStatus NOTIFY statusChanged)
public:
    explicit DownloadProcess(QObject *parent = nullptr,
                             QString enginePath = "",
                             QString UUID = "",
                             QMap<QString, QString> download_params = QMap<QString, QString>());
    ~DownloadProcess();

    enum ProcessState {
        NotRunning,
        Starting,
        Running
    };
    Q_ENUM(ProcessState)

    enum Status {
        Finished,
        Failed,
        Queued
    };
    Q_ENUM(Status)

    DownloadProcess::ProcessState state;
    QMap<QString,QString> downloadProgress;
    DownloadProcess::Status status;

public slots:
    DownloadProcess::ProcessState getState() const;
    QMap<QString,QString> getDownloadProgress();
    DownloadProcess::Status getStatus();
    void close();
    void closeAndDelete();
    void start();
    bool isRunning();
    QString getVideoId();
    QString getPlaylistId();

    void stop();
    void changeStatus(QString status);
    void destroy();
signals:
    void formatReady();
    void stopped();
    void stateChanged(DownloadProcess::ProcessState processState);
    void downloadProgressChanged(QMap<QString,QString> downloadProgress);
    void statusChanged(DownloadProcess::Status status);

    void downloadFailed();
    void downloadFinished();

private slots:
    void setDownloadProgress(QMap<QString,QString> downloadProgress);
    void setState(DownloadProcess::ProcessState processState);
    void setStatus(DownloadProcess::Status status);

    void downloadProcessReadyRead();
    void downloadProcessFinished(int exitCode);

    void startDownloadProcess();
    void startGetFormatsProcess();
    void formatProcessFinished(int exitCode);
    QStringList getDownloadArgs();
    void setDownloadArgs(QStringList audioFormatCodes,QStringList videoFormatCodes);
    void init_progressFile();
    int roundFormat(int availableFormatsCount, int selectedFormatValue);
    int roundAudioFormat(int selectedFormatValue);
private:
    QString enginePath;
    QString uuid;
    QString videoId;
    QString parent_playlistId;
    QMap<QString, QString> download_params;
    QSettings *settings;
};

#endif // DOWNLOADPROCESS_H
