#include "playlistview.h"
#include "ui_playlistview.h"

#include "mainwindow.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QUrl>
#include <QScrollBar>


PlaylistView::PlaylistView(QWidget *parent, QVariant playlist_id) :
    QWidget(parent),
    ui(new Ui::PlaylistView)
{
    ui->setupUi(this);

    #if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
        ui->filterLineEdit->setClearButtonEnabled(true);
    #endif

    ui->downloadPushButton->setEnabled(false);

    ui->resultsListWidget->setSpacing(4);
    //ui->resultsListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->resultsListWidget->setUniformItemSizes(true);

    //find parent networkaccessmanager
    QObject *parentObj = this->parent();
    auto *manager = parentObj->findChild<QNetworkAccessManager*>();
    if(manager == nullptr) //if its not there init one
    {
        this->networkManager_ = new QNetworkAccessManager(this);
        QNetworkDiskCache *diskCache = new QNetworkDiskCache(this);
        diskCache->setCacheDirectory(QStandardPaths::writableLocation(
                                         QStandardPaths::CacheLocation));
        this->networkManager_->setCache(diskCache);
    }else{
        this->networkManager_ = manager;
    }

    // loader is the child of results
    _loader = new WaitingSpinnerWidget(ui->resultsListWidget,true,true);
    _loader->setRoundness(70.0);
    _loader->setMinimumTrailOpacity(15.0);
    _loader->setTrailFadePercentage(70.0);
    _loader->setNumberOfLines(9);
    _loader->setLineLength(12);
    _loader->setLineWidth(2);
    _loader->setInnerRadius(2);
    _loader->setRevolutionsPerSecond(3);
    _loader->setColor(QColor("#1e90ff"));

    if(!playlist_id.toString().isEmpty())
    {
        loadPlaylist(this->playlist_id);
    }
}

void PlaylistView::flat_playlist(QVariant playlist_id)
{
    //load from cache
    QString p_id = playlist_id.toString();
    QFileInfo playlistCacheFileInfo(this->playlistCacheFileName);
    if(playlistCacheFileInfo.isFile() && playlistCacheFileInfo.exists())
    {
        QString data = utils::loadJson(this->playlistCacheFileName).toJson();
        loadToView(true,data);
        return;
    }
    //load from remote
    QString url_str = "https://m.youtube.com/playlist?list="+p_id;
    QProcess *flatter = new QProcess(this);
    flatter->setProcessChannelMode(QProcess::SeparateChannels);
    connect(flatter,SIGNAL(finished(int)),this,SLOT(flatterFinished(int)));
    connect(flatter,SIGNAL(readyReadStandardError()),this,SLOT(flattererror()));


    flatter->setArguments(QStringList()<<Engine::enginePath()<<"--dump-single-json"<<"--flat-playlist"<<url_str);
    _loader->start();
    flatter->setProgram("python3");
    flatter->start();
    if(flatter->waitForStarted() == false){
        flatter->setProgram("python3");
        flatter->start();
    }
}

void PlaylistView::flattererror()
{
    QProcess *flatter = static_cast<QProcess*>(sender());
    if(flatter != nullptr)
    {
        QString data = flatter->readAllStandardError();
        QMessageBox::critical(this,QApplication::applicationName()+" | "+tr("Error"),
                              "An error occured while processing playlist.\n\nError code "+QString::number(flatter->exitCode())+"\n\n"+data);
    }
}

void PlaylistView::flatterFinished(int exitCode)
{
    _loader->stop();
    QProcess *flatter = static_cast<QProcess*>(sender());
    if(flatter != nullptr)
    {
        if(exitCode==0)
        {
            QString data = flatter->readAll();
            loadToView(false,data);
        }else {
            qWarning()<<"ftr exited with code"<<exitCode;
        }
    }
    updateStatusLabel();
}

void PlaylistView::loadToView(const bool fromCache, const QString data)
{
    QObject *mainWindowObject = utils::getMainWindow(this);
    MainWindow  *mainWindow = dynamic_cast<MainWindow *>(mainWindowObject);

    mainWindow->startSpinner();

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());

    if(!fromCache && !jsonResponse.isEmpty())
        utils::saveJson(jsonResponse,playlistCacheFileName);

    QJsonObject jsonObject = jsonResponse.object();
    QString playlist_id    = jsonObject.value("id").toString();
    QString playlist_name  = jsonObject.value("title").toString();
    QString uploader_name  = jsonObject.value("uploader").toString();

    this->playlist_author = uploader_name;

    ui->playAuthotUploads->setEnabled(true);
    ui->playPlaylist->setEnabled(true);
    ui->copyId->setEnabled(true);

    this->setWindowTitle(QApplication::applicationName()+" | "+tr("Playlist - ")+playlist_name);
    ui->name_label->setText(playlist_name);
    ui->author_name_label->setText(uploader_name);
    ui->id_label->setText(playlist_id);

    int videoAdded = 0;
    QJsonArray jsonArray = jsonObject["entries"].toArray();

    ui->resultsListWidget->setUpdatesEnabled(false);
    foreach (const QJsonValue & value, jsonArray)
    {
        QJsonObject obj = value.toObject();
        QString videoId, title, thumb, durationStr;
        int duration;
        videoId = obj.value("id").toString();
        title   = obj.value("title").toString();
        thumb   = "https://i.ytimg.com/vi/"+videoId+"/mqdefault.jpg";
        duration   = obj.value("duration").toInt();
        durationStr= utils::formatSeconds(duration);

        if(title.contains("[Deleted video]",Qt::CaseInsensitive) ||
                title.contains("[Private video]",Qt::CaseInsensitive))
        {
            qDebug()<<"Deleted Video" << videoId;
        }else{
            videoAdded++;
            //init itemwidget & add to resultList
            VideoItem *videoItem = new VideoItem(ui->resultsListWidget,this->networkManager_);
            videoItem->setObjectName("video_"+videoId);
            connect(videoItem,&VideoItem::itemCheckedChanged,this,&PlaylistView::updateStatusLabel);

            videoItem->init(videoAdded,videoId,title,thumb,durationStr);
            videoItem->adjustSize();
            QListWidgetItem* item;
            item = new QListWidgetItem(ui->resultsListWidget);
            ui->resultsListWidget->setItemWidget(item, videoItem);
            item->setSizeHint(videoItem->sizeHint());
            ui->resultsListWidget->addItem(item);
        }
        if(mainWindow->quiting == true){
            break;
        }
        QApplication::processEvents();
    }
    ui->resultsListWidget->setUpdatesEnabled(true);

    ui->video_count_label->setText(QString::number(videoAdded));

    mainWindow->stopSpinner();
}

PlaylistView::~PlaylistView()
{
    delete ui;
}

void PlaylistView::resetUi()
{
    ui->playAuthotUploads->setEnabled(false);
    ui->playPlaylist->setEnabled(false);
    ui->copyId->setEnabled(false);

    ui->selectAllCheckBox->setChecked(false);

    ui->resultsListWidget->clear();
    ui->resultsListWidget->verticalScrollBar()->setValue(
                ui->resultsListWidget->verticalScrollBar()->minimum());

    updateStatusLabel();
    ui->name_label->setText("");
    ui->author_name_label->setText("");
    ui->id_label->setText("");
    ui->video_count_label->setText("");
    _loader->stop();
}

void PlaylistView::loadPlaylist(const QString playlistId)
{
    this->resetUi();

    this->playlist_id = playlistId;
    this->playlistCacheFileName = utils::returnPath("playlist_cache")+this->playlist_id;

    this->flat_playlist(playlistId);
}

void PlaylistView::on_selectAllCheckBox_toggled(bool checked)
{
    this->selectAllItemsInView(ui->resultsListWidget,checked);
}

void PlaylistView::selectAllItemsInView(QListWidget *listWidget,bool checked)
{
    int total = listWidget->count();
    for (int i = 0; i < total; ++i)
    {
        QListWidgetItem *item = listWidget->item(i);
        if(item->isHidden() == false)
        {
            QWidget *itemWidget = listWidget->itemWidget(item);
            QCheckBox *selectionCheckBox = itemWidget->findChild<QCheckBox*>();
            selectionCheckBox->setChecked(checked);
        }
    }
}

void PlaylistView::updateStatusLabel()
{
    int checkedCount = checkItemCount(ui->resultsListWidget);
    ui->status_label->setText(QString::number(checkedCount)+QString(checkedCount == 1 ? tr(" item selected"):tr(" items selected")));

    //enable disable download button
    ui->downloadPushButton->setEnabled((checkedCount > 0));
}

int PlaylistView::checkItemCount(QListWidget *listWidget)
{
    int count = 0;

    int total = listWidget->count();
    for (int i = 0; i < total; ++i)
    {
        QListWidgetItem *item = listWidget->item(i);
        QWidget *itemWidget = listWidget->itemWidget(item);
        QCheckBox *selectionCheckBox = itemWidget->findChild<QCheckBox*>();
        if(selectionCheckBox != nullptr && selectionCheckBox->isChecked())
            count++;

    }
    return count;
}


//FILTER=====================================================================================
void PlaylistView::filterList(const QString &arg1,QListWidget *listWidget)
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
void PlaylistView::fillFilteredItemsMetaList(QListWidget *listWidget)
{
    for(int i= 0 ; i <listWidget->count(); i++)
    {
        QListWidgetItem *item = listWidget->item(i);
        ElidedLabel *title = listWidget->itemWidget(item)->findChild<ElidedLabel *>("title");
        QString titleStr = static_cast<ElidedLabel*>(title)->text();
        filteredItemsMetaList.append(titleStr);
    }
}

void PlaylistView::hideAllListItems(QListWidget *listWidget)
{
    for(int i = 0 ; i < listWidget->count();i++)
    {
        listWidget->item(i)->setHidden(true);
    }
}
//END FILTER==================================================================================


void PlaylistView::on_filterLineEdit_textChanged(const QString &arg1)
{
    filterList(arg1,ui->resultsListWidget);
}

void PlaylistView::on_resultsListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    QWidget *itemWidget = ui->resultsListWidget->itemWidget(item);
    QCheckBox *selectionCheckBox = itemWidget->findChild<QCheckBox*>();
    selectionCheckBox->toggle();
}

QStringList PlaylistView::getSelectedItemId(QListWidget *listWidget)
{
    QStringList itemList;
    int total = listWidget->count();
    for (int i = 0; i < total; ++i)
    {
        QListWidgetItem *item = listWidget->item(i);
        QWidget *itemWidget = listWidget->itemWidget(item);
        QCheckBox *selectionCheckBox = itemWidget->findChild<QCheckBox*>();
        if(selectionCheckBox->isChecked())
        {
            VideoItem *videoItem  = qobject_cast<VideoItem*>(itemWidget);
            itemList.append(videoItem->getVideoId());
        }
    }
    return itemList;
}

void PlaylistView::on_downloadPushButton_clicked()
{
    //--yes-playlist --playlist-items "3" -f "160+249" --merge-output-format "mp4" -o "/tmp/test3/%(playlist_index)s-%(title)s-%(id)s.%(ext)s" --exec "mv {} /tmp/test/" PLq3UZa7STrbpX13PljcNH6hmyrSbcMYK8

    emit downloadSelected(this->playlist_id,getSelectedItemId(ui->resultsListWidget));
}

void PlaylistView::on_playPlaylist_clicked()
{
    emit playPlaylist(this->playlist_id);
}

void PlaylistView::on_playAuthotUploads_clicked()
{
    emit playAuthorUploads(this->playlist_author);
}

void PlaylistView::on_copyId_clicked()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText("https://www.youtube.com/playlist?list="+this->playlist_id);
}
