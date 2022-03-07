#include "downloadwidget.h"
#include "ui_downloadwidget.h"

#include "mainwindow.h"

#include <QGraphicsOpacityEffect>
#include <QScrollBar>
#include "engine.h"
#include "helper.h"
#include "playlistinfo.h"


DownloadWidget::DownloadWidget(QWidget *parent, QNetworkAccessManager *manager) :
    QWidget(parent),
    ui(new Ui::DownloadWidget)
{
    ui->setupUi(this);

    ui->startStopButton->setEnabled(false);
    ui->removeButton->setEnabled(false);

    this->networkManager_ = manager;
    this->download_record_dir_path = utils::returnPath("download_records");

    init_downloadManager();

    #if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
        ui->playlistFilterLineEdit->setClearButtonEnabled(true);
    #endif

    foreach (QListWidget *listWidget, this->findChildren<QListWidget*>()) {
        listWidget->setSpacing(4);
        listWidget->setUniformItemSizes(true);
        //listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    ui->stackedWidget->setCurrentWidget(ui->playlistItemsWidget);

    loadHistory();

    ui->itemInfoWidget->hide();

    ui->playlistTitle->installEventFilter(this);
}


void DownloadWidget::init_downloadManager()
{
    downloadManager = new DownloadManager(this,Engine::enginePath());

    connect(downloadManager,&DownloadManager::downloadProcessListChanged,[=]()
    {
        QTimer::singleShot(500,this,SLOT(updateButtons()));
    });

    connect(downloadManager,&DownloadManager::processStatusChanged,[=](DownloadProcess::Status status,DownloadProcess *downloadProcess){
        if( downloadProcess != nullptr)
        {
            auto pUUID = downloadProcess->getPlaylistId();
            QWidget *itemWidget = ui->playlistWidget->findChild<QWidget*>("item_" + pUUID);
            if(itemWidget == nullptr)
                return;
            PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
            if(status == DownloadProcess::Status::Finished){
                //finished
                playlistItem->appendFinished();
            }else if (status == DownloadProcess::Status::Failed) {
                //failed
                playlistItem->appendFailed();
            }else{
                //queued
                playlistItem->appendQueued();
            }
        }
    });

    connect(downloadManager,&DownloadManager::addedNewDownloadProcess,[=](DownloadProcess *dp)
    {
        if(ui->stackedWidget->currentWidget() != ui->playlistViewWidget)
            return;

        //update download items
        for (int i = 0; i < ui->playlistViewlistWidget->count(); i++)
        {
            auto item = ui->playlistViewlistWidget->item(i);
            auto downloadItem = dynamic_cast<DownloadItem*>(ui->playlistViewlistWidget->itemWidget(item));

            if (downloadItem->objectName() == "item_"+dp->getVideoId())
            {
                if(downloadItem != nullptr){
                    downloadItem->setState(dp->getState());
                    downloadItem->setStatus(dp->getStatus());
                    if( !downloadItem->hasDownloadProcess()){
                        downloadItem->setProcess(dp);
                    }
                }
                break;
            }
        }
    });
}

void DownloadWidget::updatePlaylistItems()
{
     //QList<DownloadProcess*>  processList = downloadManager->getDownloadProcessList();
    /**
     * This implimentation is based on selective items method, only items that are downlaodmanager will recieve update
    **/
    QStringList playListsInDownlaodManager = downloadManager->getPlaylistsBeingDownloaded();
    //include current item cause current id will be removed in getPlaylistsBeingDownloaded when stop is clicked
    const int currentRow = ui->playlistWidget->currentRow();
    QListWidgetItem *item = ui->playlistWidget->item(currentRow);
    if(item != nullptr){
         QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
         PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
         playListsInDownlaodManager.append(playlistItem->getPlaylistUUID());
    }
    foreach (QString pUUID, playListsInDownlaodManager)
    {

       QWidget *itemWidget = ui->playlistWidget->findChild<QWidget*>("item_" + pUUID);
       if(itemWidget == nullptr)
           return;
       PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
       QString playlistId  = itemWidget->objectName().split("item_").last();


       if(downloadManager->isDownloadingPlaylist(playlistId))
       {
           playlistItem->setStatus(PlaylistEntryItem::Status::Running);

       }else if(downloadManager->isInDownloadingQueue(playlistId)){

           playlistItem->setStatus(PlaylistEntryItem::Status::Queued);
       }else{

           playlistItem->setStatus(PlaylistEntryItem::Status::NotRunning);
       }

//       int running = 0;
//       int count   = 0;

//       foreach (auto proc, processList) {
//           qDebug()<<"PROC NAME:"<<proc->objectName()<<"PLAYLISID:"<<playlistId;
//           if(proc->objectName().contains(playlistId)){
//               count = count + 1;
//               if(proc->isRunning()){
//                   running =  running + 1;
//               }
//           }
//       }

//       QString status = "Stalled";

//       if(downloadManager->isDownloadingPlaylist(playlistId))
//       {
//           status = QString("Running: %1 of %2").arg(QString::number(running),QString::number(count));
//       }else if(downloadManager->isInDownloadingQueue(playlistId) && running == 0)
//       {
//           status = "Queued";
//       }
//       playlistItem->setStatusText(status);
    }

    /**
     * This implementation will update all items in playlistWidget, hence is expensive
    **/
    //    for (int i = 0; i < ui->playlistWidget->count(); ++i)
    //    {
    //        QListWidgetItem *item = ui->playlistWidget->item(i);
    //        QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
    //        PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
    //        QString playlistId  = itemWidget->objectName().split("item_").last();
    //        if(downloadManager->isDownloadingPlaylist(playlistId))
    //        {
    //            playlistItem->setStatus(PlaylistEntryItem::Status::Running);
    //        }else{
    //            playlistItem->setStatus(PlaylistEntryItem::Status::NotRunning);
    //        }
    //    }

}

void DownloadWidget::updateButtons()
{
    updatePlaylistItems();

    QListWidgetItem *item = ui->playlistWidget->currentItem();
    if(item == nullptr){
        ui->removeButton->setEnabled(false);
        ui->startStopButton->setEnabled(false);
        return;
    }
    QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
    PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
    if(playlistItem != nullptr)
    {
        QFileInfo f_info(playlistItem->getPlaylistRecordFileName());
        QString UUID = f_info.baseName(); // template : playlistId__S__timeadded;
        if(downloadManager->isDownloadingPlaylist(UUID))
        {
            ui->startStopButton->setEnabled(true);
            ui->startStopButton->setText("Stop");
            ui->startStopButton->setIcon(QIcon(":/icons/others/primo/red/button_blue_stop.png"));

        }else{
            ui->startStopButton->setEnabled(true);
            ui->startStopButton->setText("Start");
            ui->startStopButton->setIcon(QIcon(":/icons/others/primo/button_blue_play.png")); 
        }
        ui->removeButton->setEnabled(true);
    }
}


bool DownloadWidget::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == ui->playlistTitle){
       if( event->type()==QEvent::Enter){
           ui->playlistTitle->resume();
       }
       if( event->type()==QEvent::Leave){
           ui->playlistTitle->pause();
       }
    }
    return QWidget::eventFilter(watched,event);
}

/**
 * @brief DownloadWidget::get_download_record_dir_path
 * @return the directory path where we keep download records files
 */
QString DownloadWidget::get_download_record_dir_path()
{
    return this->download_record_dir_path;
}


void DownloadWidget::loadHistory()
{
    QDir download_record_dir(get_download_record_dir_path());
    download_record_dir.setFilter(QDir::Files);
    download_record_dir.setNameFilters(QStringList()<<"*.json");
    download_record_dir.setSorting(QDir::Time | QDir::Reversed);

    QFileInfoList record_files_info = download_record_dir.entryInfoList();
    foreach (const QFileInfo record_file_info, record_files_info) {
        addToDownload(record_file_info.filePath(), false);
    }
}


void DownloadWidget::addToDownload(QString download_record_filename, bool animate )
{
    QFileInfo f_info(download_record_filename);
    QString UUID = f_info.baseName();
    qint64 added = QString(QString(UUID).split("__S__").last()).toLongLong();
    QDateTime addedDateTime = QDateTime::fromMSecsSinceEpoch(added);
    QString addedStr = addedDateTime.toLocalTime().toString();


    QJsonDocument  recordJson = utils::loadJson(download_record_filename);
    if(recordJson.isEmpty())
        return;
    QJsonObject docobj       =   recordJson.object();
    QJsonArray  metaArray    =   docobj.value("playlist_meta").toArray();
    QJsonArray  itemsArray   =   docobj.value("items").toArray();
    QJsonArray  paramArray   =   docobj.value("download_params").toArray();

    int videoCount = itemsArray.count();

    QString firstVidId;
    if(itemsArray.isEmpty() == false)
        firstVidId = itemsArray.first().toObject().value("id").toString();
    QString playlistThumbnail = "https://i.ytimg.com/vi/"+firstVidId+"/mqdefault.jpg";

    auto valObj = metaArray.first().toObject();
    QString playlist_id = valObj.value("id").toString();
    QString playlist_name = valObj.value("title").toString();
    QString playlist_uploader = valObj.value("uploader").toString();
    QString playlist_uploader_id = valObj.value("uploader").toString();

    auto paramObj = paramArray.first().toObject();
    QString download_type           = paramObj.value("download_type").toString();
    QString container               = paramObj.value("container").toString();
    QString download_location       = paramObj.value("download_location").toString();

    QList<int>quality;
    int audioQuality                = paramObj.value("audio_quality").toInt();
    quality.append(audioQuality);
    if(paramObj.value("video_quality").isUndefined() == false){
        int videoQuality      = paramObj.value("video_quality").toInt();
        quality.append(videoQuality);
    }

    //init itemwidget & add to resultList
    PlaylistEntryItem *playlistItem = new PlaylistEntryItem(ui->playlistWidget,networkManager_);
    if(animate)
        playlistItem->setWindowOpacity(0.0);


    playlistItem->setObjectName("item_"+UUID);
    playlistItem->init(download_record_filename,playlist_id,playlist_name,playlistThumbnail,playlist_uploader
                       ,playlist_uploader_id,videoCount,addedStr,quality,container,download_type,download_location);

    playlistItem->adjustSize();
    QListWidgetItem* item;
    item = new QListWidgetItem(ui->playlistWidget);
    ui->playlistWidget->setItemWidget(item, playlistItem);
    item->setSizeHint(playlistItem->sizeHint());
    ui->playlistWidget->addItem(item);

    connect(playlistItem,&PlaylistEntryItem::selectItem,[=](QPoint itemPos)
    {
       ui->playlistWidget->setCurrentItem(ui->playlistWidget->itemAt(itemPos));
    });

    connect(playlistItem,&PlaylistEntryItem::viewPlaylist,[=](QString playlistId)
    {
        Q_UNUSED(playlistId)
        updatePlaylisInfoButton(playlistItem);
        loadPlaylist(download_record_filename);
    });

    connect(playlistItem,&PlaylistEntryItem::viewPlaylistInfo,[=]()
    {
        showPlaylistInfo(download_record_filename);
    });

    ui->playlistWidget->setCurrentRow(ui->playlistWidget->count()-1);
    if(animate){
        playlistItem->animate();
    }
}

void DownloadWidget::loadPlaylist(QString download_record_filename)
{
    QObject *mainWindowObject = utils::getMainWindow(this);
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(mainWindowObject);
    mainWindow->startSpinner();

    QFileInfo f_info(download_record_filename);
    QString UUID = f_info.baseName();
    qint64 added = QString(QString(UUID).split("__S__").last()).toLongLong();
    QDateTime addedDateTime = QDateTime::fromMSecsSinceEpoch(added);
    QString addedStr        = addedDateTime.toLocalTime().toString();

    //ui->playlistViewlistWidget->clear();
    //scroll to top
    ui->playlistViewlistWidget->verticalScrollBar()->setValue(
                ui->playlistViewlistWidget->verticalScrollBar()->minimum());

    QJsonDocument  recordJson = utils::loadJson(download_record_filename);
    if(recordJson.isEmpty())
        return;
    QJsonObject docobj       =   recordJson.object();
    QJsonArray  metaArray    =   docobj.value("playlist_meta").toArray();
    QJsonArray  itemsArray   =   docobj.value("items").toArray();
    QJsonArray  paramArray   =   docobj.value("download_params").toArray();

    auto valObj = metaArray.first().toObject();
    QString playlist_id         = valObj.value("id").toString();
    QString playlist_name       = valObj.value("title").toString();
    QString playlist_uploader   = valObj.value("uploader").toString();


    auto paramObj                   = paramArray.first().toObject();
    QString download_type           = paramObj.value("download_type").toString();

    QString container               = paramObj.value("container").toString();
    QList<int>quality;
    int audioQuality                = paramObj.value("audio_quality").toInt();
    quality.append(audioQuality);
    if(paramObj.value("video_quality").isUndefined() == false){
        int videoQuality      = paramObj.value("video_quality").toInt();
        quality.append(videoQuality);
    }
    QString formatStr;
    if(quality.count() == 2){
        formatStr.append("Audio quality: "+Helper::getFormatName(quality.at(0)));
        formatStr.append(", ");
        formatStr.append("Video quality: "+Helper::getFormatName(quality.at(1)));
    }else if(quality.count() == 1){
        formatStr.append("Audio quality: "+Helper::getFormatName(quality.at(0)));
    }

    ui->playlistTitle->setText("Name: "+playlist_name +" | By: "+playlist_uploader
                               +" | Format: "+download_type.toUpper()
                               +"("+container+")");

    ui->playlistViewlistWidget->setUpdatesEnabled(false);
    foreach (QJsonValue val, itemsArray)
    {
        QJsonObject valObj  = val.toObject();
        QString title       = valObj.value("title").toString();
        QString id          = valObj.value("id").toString();
        QString thumb       = "https://i.ytimg.com/vi/"+id+"/mqdefault.jpg";
        int duration        = valObj.value("duration").toInt();

        QString durationStr = utils::formatSeconds(duration);

        if(!title.contains("[Deleted video]",Qt::CaseInsensitive) ||
                !title.contains("[Private video]",Qt::CaseInsensitive))
        {
            QString itemUUID = UUID+"__V__"+id;

            DownloadItem *downloadItem = new DownloadItem(ui->playlistViewlistWidget,itemUUID);
            downloadItem->setObjectName("item_"+id);
            downloadItem->setToolTip("Video id: "+id);
            downloadItem->init(id,title,thumb,durationStr);
            downloadItem->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            downloadItem->adjustSize();


            QListWidgetItem* item;
            item = new QListWidgetItem(ui->playlistViewlistWidget);
            ui->playlistViewlistWidget->setItemWidget(item, downloadItem);
            item->setSizeHint(downloadItem->sizeHint());
            ui->playlistViewlistWidget->addItem(item);

            //connect to existing processes in download manager
            DownloadProcess *dp = downloadManager->getDownloadProcess(itemUUID);
            if(dp != nullptr){
                downloadItem->setState(dp->getState());
                downloadItem->setStatus(dp->getStatus());
                if( !downloadItem->hasDownloadProcess()){
                    downloadItem->setProcess(dp);
                }
            }
        }
        if(mainWindow->quiting == true){
            break;
        }
        QApplication::processEvents();
    }
    ui->playlistViewlistWidget->setUpdatesEnabled(true);

    ui->stackedWidget->slideInWgt(ui->playlistViewWidget);
    mainWindow->stopSpinner();
    mainWindow = nullptr;
    delete mainWindow;
}

DownloadWidget::~DownloadWidget()
{
    delete ui;
}

void DownloadWidget::home()
{
    ui->stackedWidget->slideInWgt(ui->playlistItemsWidget);
}

void DownloadWidget::on_backToPlaylistButton_clicked()
{
    ui->playlistViewlistWidget->clear(); //to stop updates to item
    ui->stackedWidget->slideInWgt(ui->playlistItemsWidget);
}

void DownloadWidget::on_playlistWidget_itemDoubleClicked(QListWidgetItem *item)
{
    QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
    PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);

    updatePlaylisInfoButton(playlistItem);

    if(playlistItem != nullptr){
        this->loadPlaylist(playlistItem->getPlaylistRecordFileName());
    }
}

void DownloadWidget::updatePlaylisInfoButton(PlaylistEntryItem *playlistItem)
{
    ui->playlistInfoButton->disconnect();

    if(playlistItem != nullptr)
    {
        connect(ui->playlistInfoButton,&QPushButton::clicked,[=](){
           showPlaylistInfo(playlistItem->getPlaylistRecordFileName());
        });
    }
}

void DownloadWidget::on_startStopButton_clicked()
{
    ui->startStopButton->setEnabled(false);

    const int currentRow = ui->playlistWidget->currentRow();
    QListWidgetItem *item = ui->playlistWidget->item(currentRow);
    if(item != nullptr){
        QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
        PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
        QFileInfo f_info(playlistItem->getPlaylistRecordFileName());
        QString UUID = f_info.baseName();
        if(downloadManager->isDownloadingPlaylist(UUID)){
            qDebug()<<"STOPPING DOWNLOADING"<<UUID;
            downloadManager->stopDownload(UUID);
            playlistItem->resetQueued();
        }else{
            qDebug()<<"STARTING DOWNLOADING"<<UUID;
            downloadManager->addDownload(playlistItem->getPlaylistRecordFileName());
            playlistItem->resetFailed();
            playlistItem->setStatus(PlaylistEntryItem::Status::Queued);
        }
    }
    QTimer::singleShot(500,this,SLOT(updateButtons()));
}

void DownloadWidget::on_removeButton_clicked()
{
     const int currentRow  = ui->playlistWidget->currentRow();
     QListWidgetItem *item = ui->playlistWidget->item(currentRow);
     if(item != nullptr){
         QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
         PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
         QFileInfo f_info(playlistItem->getPlaylistRecordFileName());
         QString UUID = f_info.baseName(); // template : playlistId__S__timeadded;

         //stop any download if running
         downloadManager->stopDownload(UUID);
         delete ui->playlistWidget->takeItem(currentRow);
         //remove record file
         QFile recordFile(f_info.filePath());
         recordFile.remove();
         //remove progress directory
         QString path = QString("progress")+QDir::separator()+UUID;
         QString progresFilePath = utils::returnPath(path);
         QDir progressDir(progresFilePath);
         progressDir.removeRecursively();
     }
}

void DownloadWidget::on_playlistWidget_currentRowChanged(int currentRow)
{
    QListWidgetItem *item = ui->playlistWidget->item(currentRow);
    if(item == nullptr){
        ui->removeButton->setEnabled(false);
        ui->startStopButton->setEnabled(false);
        return;
    }
    QWidget *itemWidget = ui->playlistWidget->itemWidget(item);
    PlaylistEntryItem *playlistItem = qobject_cast<PlaylistEntryItem*>(itemWidget);
    if(playlistItem != nullptr){
        auto UUID = playlistItem->getPlaylistUUID();
        if(downloadManager->isDownloadingPlaylist(UUID))
        {
            ui->startStopButton->setEnabled(true);
            ui->startStopButton->setText("Stop");
            ui->startStopButton->setIcon(QIcon(":/icons/others/primo/red/button_blue_stop.png"));
        }else{
            ui->startStopButton->setEnabled(true);
            ui->startStopButton->setText("Start");
            ui->startStopButton->setIcon(QIcon(":/icons/others/primo/button_blue_play.png"));
        }
        ui->removeButton->setEnabled(true);
    }
}

void DownloadWidget::showPlaylistInfo(QString download_record_filename)
{
    PLaylistInfo *playlistInfo = new PLaylistInfo(this,download_record_filename);
    playlistInfo->setWindowFlag(Qt::Dialog);
    playlistInfo->setWindowTitle(QApplication::applicationName()+" | "+tr("Playlist Details"));

    playlistInfo->show();
}

void DownloadWidget::on_playlistViewlistWidget_currentRowChanged(int currentRow)
{
    ui->thumbnail->disconnect();
    QListWidgetItem *item = ui->playlistViewlistWidget->item(currentRow);
    if(item == nullptr){
        ui->itemInfoWidget->hide();
        return;
    }
    ui->itemInfoWidget->show();
    QWidget *itemWidget = ui->playlistViewlistWidget->itemWidget(item);
    DownloadItem *videoItem = qobject_cast<DownloadItem*>(itemWidget);
    if(videoItem != nullptr){
        QString videoId     = videoItem->getVideoId();
        QString videoTitle  = videoItem->getVideoTitle();
        QString thumb       = videoItem->getThumbUrl();
        QString durationStr = videoItem->getDurationStr();
        ui->duration->setText(durationStr);
        ui->title->setText(videoTitle);
        ui->id->setText(videoId);
        ui->thumbnail->setPixmap(QPixmap(":/icons/others/wall_placeholder_180.jpg").scaled(ui->thumbnail->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->thumbnail->init(this->networkManager_,thumb,":/icons/others/wall_placeholder_180.jpg");
    }
}

void DownloadWidget::on_playlistFilterLineEdit_textChanged(const QString &arg1)
{
    this->filterList(arg1,ui->playlistWidget);
}

//FILTER=====================================================================================
void DownloadWidget::filterList(const QString &arg1,QListWidget *listWidget)
{
    hideAllListItems(listWidget);
    filteredItemsMetaList.clear();
    fillFilteredItemsMetaList(listWidget);

    for(int i= 0 ; i < filteredItemsMetaList.count(); i++)
    {
        if(QString(filteredItemsMetaList.at(i)).contains(arg1,Qt::CaseInsensitive))
        {
            QListWidgetItem *item = listWidget->item(i);
            item->setHidden(false);
        }
    }
}
void DownloadWidget::fillFilteredItemsMetaList(QListWidget *listWidget)
{
    for(int i= 0 ; i <listWidget->count(); i++)
    {
        QListWidgetItem *item = listWidget->item(i);
        ElidedLabel *title = listWidget->itemWidget(item)->findChild<ElidedLabel *>("title");
        QString titleStr = static_cast<ElidedLabel*>(title)->text();
        QLabel *by = listWidget->itemWidget(item)->findChild<QLabel *>("by");
        QString byStr = static_cast<ElidedLabel*>(by)->text();
        filteredItemsMetaList.append(titleStr + " " +byStr);
    }
}

void DownloadWidget::hideAllListItems(QListWidget *listWidget)
{
    for(int i = 0 ; i < listWidget->count();i++)
    {
        listWidget->item(i)->setHidden(true);
    }
}
//END FILTER==================================================================================

