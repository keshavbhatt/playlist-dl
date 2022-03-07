#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QProcess>
#include <QSettings>

#include "downloadprocess.h"

class DownloadManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<DownloadProcess*> downloadProcessList READ getDownloadProcessList WRITE setDownloadProcessList NOTIFY downloadProcessListChanged)
public:
    explicit DownloadManager(QObject *parent = nullptr,QString enginePath = "");

signals:
    void runningProcessCountChanged(int count);
    void addedNewDownloadProcess(DownloadProcess *dp);
    void downloadProcessListChanged();
    void processStatusChanged(DownloadProcess::Status status,DownloadProcess *downloadProcess);


public slots:
    bool isDownloadingPlaylist(const QString UUID);
    bool isInDownloadingQueue(const QString UUID);

    void stopDownload(const QString UUID);

    void addDownload(QString download_record_filename);
    DownloadProcess *getDownloadProcess(QString itemUUID);
    QList<DownloadProcess*> getDownloadProcessList();
    QStringList getPlaylistsBeingDownloaded();
private slots:
    int  getRunningProcessCount();
    void startDownloader();
    //bool isDownloaded(QString videoId, QString UUID);


    void initVideoDownloadProcess(QString UUID,QMap<QString,QString> download_params);
    void downloadProcessStopped();
    void removePlaylistFromDownloadQueue(const QString UUID);
    int  get_concurrentDownloadProcessLimit();

    void setDownloadProcessList(QList<DownloadProcess *> downloadProcessList);
    void downloadProcessStatusChanged(DownloadProcess::Status status);
private:
    QList<DownloadProcess*> downloadProcessList;
    QString enginePath;
    int concurrentDownloadProcessLimit;
    QSettings settings;
};

#endif // DOWNLOADMANAGER_H
