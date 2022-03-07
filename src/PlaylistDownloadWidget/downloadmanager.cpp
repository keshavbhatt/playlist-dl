#include "downloadmanager.h"
#include "utils.h"
#include "helper.h"

#include <QFileInfo>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>


DownloadManager::DownloadManager(QObject *parent, QString enginePath) : QObject(parent)
{
    this->enginePath = enginePath;
    this->concurrentDownloadProcessLimit = settings.value("concurrent_downloads",2).toInt();
}

bool DownloadManager::isDownloadingPlaylist(const QString UUID)
{
    bool running= false;
    QList<DownloadProcess*> allDownloadProcesses = this->findChildren<DownloadProcess*>();
    foreach (DownloadProcess *process, allDownloadProcesses) {
        auto processState = process->getState();
        if(process->objectName().contains(UUID,Qt::CaseSensitive)
                && ( processState== DownloadProcess::ProcessState::Running
                    || processState == DownloadProcess::ProcessState::Starting)){
            running = true;
            break;
        }
    }
    return running;
}

bool DownloadManager::isInDownloadingQueue(const QString UUID)
{
    bool inqueue= false;
    QList<DownloadProcess*> allDownloadProcesses = this->findChildren<DownloadProcess*>();
    foreach (DownloadProcess *process, allDownloadProcesses) {
        if(process->objectName().contains(UUID,Qt::CaseSensitive)
                && process->getStatus() == DownloadProcess::Status::Queued){
            inqueue = true;
            break;
        }
    }
    return inqueue;
}

void DownloadManager::removePlaylistFromDownloadQueue(const QString UUID)
{
    foreach (DownloadProcess *process, downloadProcessList) {
        if(process->objectName().contains(UUID,Qt::CaseSensitive)){
            if(process->getStatus() == DownloadProcess::Status::Queued)
                process->changeStatus("idle"); // change queued status to idle
            process->stop();
            downloadProcessList.removeOne(process);
            if(process)
                process->deleteLater();
            emit downloadProcessListChanged();
        }
    }
}

void DownloadManager::stopDownload(const QString UUID)
{
    removePlaylistFromDownloadQueue(UUID);

    QList<DownloadProcess*> allDownloadProcesses = this->findChildren<DownloadProcess*>();
    foreach (DownloadProcess *process, allDownloadProcesses) {
        if(process->objectName().contains(UUID,Qt::CaseSensitive)){
            process->destroy();
        }
        emit downloadProcessListChanged();
    }
    qDebug()<<"Download Manager queue:"<<downloadProcessList;
    //continue downloading other in quque
    startDownloader();
}

void DownloadManager::addDownload(QString download_record_filename)
{
    QFileInfo f_info(download_record_filename);
    QString UUID = f_info.baseName(); // template : playlistId__S__timeadded;

    //read json file
    QJsonDocument recordJson = utils::loadJson(download_record_filename);
    if(recordJson.isEmpty()){
        qDebug()<<"DOWNLOAD RECORD JSON EMPTY";
        return;
    }
    QJsonObject docobj       =   recordJson.object();
    QJsonArray  metaArray    =   docobj.value("playlist_meta").toArray();
    QJsonArray  itemsArray   =   docobj.value("items").toArray();
    QJsonArray  dparamsArray =   docobj.value("download_params").toArray();

    auto dparams_obj = dparamsArray.first().toObject();

    QMap<QString,QString>download_params;
    QString download_type = dparams_obj.value("download_type").toString();
    QString container     = dparams_obj.value("container").toString();
    int video_quality,audio_quality;
    audio_quality = dparams_obj.value("audio_quality").toInt();
    QString download_location = dparams_obj.value("download_location").toString();

    download_params.insert("download_location",download_location);
    download_params.insert("download_type",download_type);
    download_params.insert("container",container);
    download_params.insert("audio_quality",QString::number(audio_quality));

    if(download_type == "video"){
        video_quality = dparams_obj.value("video_quality").toInt();
        download_params.insert("video_quality",QString::number(video_quality));
    }

    auto valObj = metaArray.first().toObject();
    QString playlist_id = valObj.value("id").toString();
    QString playlist_name = valObj.value("title").toString();
    QString playlist_uploader = valObj.value("uploader").toString();

    foreach (QJsonValue val, itemsArray)
    {
        QJsonObject valObj   = val.toObject();
        QString videoId      = valObj.value("id").toString();
        if(Helper::isDownloaded(videoId,UUID) == false)
        {
            initVideoDownloadProcess(UUID+"__V__"+videoId,download_params);
        }else{
            qDebug()<<videoId<<"Already Downloaded";
        }
    }
    emit downloadProcessListChanged();
}


void DownloadManager::initVideoDownloadProcess(QString UUID, QMap<QString, QString> download_params)
{
    DownloadProcess *downloadProcess = new DownloadProcess(this,this->enginePath,UUID,download_params);
    downloadProcess->setObjectName("__DP__"+UUID);
    qDebug()<<"INIT DOWNLOAD PROCESS"<<downloadProcess->objectName();
    connect(downloadProcess,SIGNAL(stopped()),this,SLOT(downloadProcessStopped()));
    connect(downloadProcess,SIGNAL(statusChanged(DownloadProcess::Status))
            ,this,SLOT(downloadProcessStatusChanged(DownloadProcess::Status)));
    downloadProcessList.append(downloadProcess);
    downloadProcess->changeStatus("queued");
    emit addedNewDownloadProcess(downloadProcess);
    startDownloader();
}

void DownloadManager::downloadProcessStatusChanged(DownloadProcess::Status status)
{
    DownloadProcess *downloadProcess =  qobject_cast<DownloadProcess*>(sender());
    if(downloadProcess == nullptr)
        return;
    if(status >= DownloadProcess::Status::Finished && status <= DownloadProcess::Status::Queued){
        emit processStatusChanged(status, downloadProcess);
    }
}


void DownloadManager::downloadProcessStopped()
{
    DownloadProcess* senderProcess = qobject_cast<DownloadProcess*>(sender());
    downloadProcessList.removeOne(senderProcess);
    senderProcess->deleteLater();

    startDownloader();
}

void DownloadManager::startDownloader()
{
    if(downloadProcessList.isEmpty()){
        qDebug()<<"Nothing to be process, downloadProcesslist is empty.";//TODO Downlaod Finsined;
    }else
    {
        foreach (DownloadProcess *process, downloadProcessList)
        {
            if(getRunningProcessCount() < this->get_concurrentDownloadProcessLimit())
            {
                //remove process from downloadProcesslist and start it
                downloadProcessList.removeOne(process);
                process->start();
            }
        }
    }
    emit downloadProcessListChanged();
}

QStringList DownloadManager::getPlaylistsBeingDownloaded()
{
    QStringList unique_playlist_ids;
    foreach (DownloadProcess *process, downloadProcessList) {
       QString pId = process->getPlaylistId();
       if(unique_playlist_ids.lastIndexOf(pId) < 0)
       {
           unique_playlist_ids<<pId;
       }
    }
    return unique_playlist_ids;
}

int DownloadManager::get_concurrentDownloadProcessLimit()
{
    return settings.value("concurrent_downloads",2).toInt();
}

QList<DownloadProcess*> DownloadManager::getDownloadProcessList()
{
    return this->downloadProcessList;
}

void DownloadManager::setDownloadProcessList(QList<DownloadProcess*> downloadProcessList)
{
    if(downloadProcessList != this->downloadProcessList){
        emit downloadProcessListChanged();
    }
}

int DownloadManager::getRunningProcessCount()
{
    int count = 0;
    foreach (DownloadProcess * downloadProcess, this->findChildren<DownloadProcess*>()) {
        if(downloadProcess->getState() == DownloadProcess::ProcessState::Running ||
                downloadProcess->getState() == DownloadProcess::ProcessState::Starting ){
            count++;
        }
    }
    return count;
}


DownloadProcess* DownloadManager::getDownloadProcess(QString itemUUID)
{
   return this->findChild<DownloadProcess*>("__DP__"+itemUUID);
}
