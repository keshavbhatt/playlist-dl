#ifndef DOWNLOADITEM_H
#define DOWNLOADITEM_H

#include <QWidget>
#include <QSettings>
#include "circularprogessbar.h"

#include <QProcess>
#include "downloadprocess.h"

namespace Ui {
class DownloadItem;
}

class DownloadItem : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadItem(QWidget *parent = nullptr, QString uid = "");
    ~DownloadItem();

    enum Status {
        Error,
        Paused,
        Running,
        Finished
    };
    Q_ENUM(Status)

public slots:
    QString getVideoId();
    QString getUUID();
    void init(QString id, QString title, QString thumb, QString durationStr);
    QString getParentPlaylistId();
    void setState(DownloadProcess::ProcessState state);
    void setProgress(QMap<QString, QString> progressMap);
    void setStatus(DownloadProcess::Status status);
    void setProcess(DownloadProcess *process);
    bool hasDownloadProcess();
    QString getVideoTitle();
    QString getThumbUrl();
    QString getDurationStr();
private slots:
    void init_progressBar();

    void loadProgress();
private:
    Ui::DownloadItem *ui;
    CircularProgessBar *circularProgressBar = nullptr;
    QString videoId, videoTitle, durationStr, thumbUrl;
    QString parent_playlistId;
    QString uuid;
    QSettings *settings;
    QMap<QString,QString> statusColor;
    DownloadProcess *dp = nullptr;
};

#endif // DOWNLOADITEM_H
