#include "playlistdownloadoptions.h"
#include "ui_playlistdownloadoptions.h"
#include "mainwindow.h"
#include "helper.h"

#include <QFileDialog>
#include <QScrollBar>

PlaylistDownloadOptions::PlaylistDownloadOptions(QWidget *parent, QNetworkAccessManager *manager, account *accountManager) :
    QWidget(parent),
    ui(new Ui::PlaylistDownloadOptions)
{
    ui->setupUi(this);

    ui->selectedItemsListWidget->setSpacing(4);
    //ui->selectedItemsListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->selectedItemsListWidget->setUniformItemSizes(true);

//    ui->selectedItemsListWidget->setSelectionMode(QAbstractItemView::NoSelection);
//    ui->selectedItemsListWidget->setSelectionRectVisible(false);

    this->networkManager_ = manager;

    if(accountManager != nullptr)
    {
        this->m_account = accountManager;
    }

    resetUi();
}

void PlaylistDownloadOptions::setAccountManager(account *accountManager)
{
    if(accountManager != nullptr)
    {
        this->m_account = accountManager;
    }
}

void PlaylistDownloadOptions::loadSelected(const QString playlistId,const QStringList itemIdList)
{
    resetUi();
    //load content
    this->playlist_id = playlistId;
    this->playlistCacheFileName = utils::returnPath("playlist_cache")+this->playlist_id;

    QString data = utils::loadJson(this->playlistCacheFileName).toJson();
    loadToView(data,itemIdList);
}

void PlaylistDownloadOptions::loadToView(const QString data, const QStringList itemIdList)
{
    QObject *mainWindowObject = utils::getMainWindow(this);
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(mainWindowObject);


    QStringList addedItemsIds;

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObject     = jsonResponse.object();
    QJsonArray jsonArray       = jsonObject["entries"].toArray();


    this->playlist_name  = jsonObject.value("title").toString();
    this->uploader_name  = jsonObject.value("uploader").toString();

    ui->selectedItemsListWidget->setUpdatesEnabled(false);
    for (int i = 0; i < jsonArray.count(); ++i)
    {
        const QJsonValue & value = jsonArray.at(i);
        QJsonObject obj = value.toObject();
        QString videoId, title, thumb, durationStr;
        int duration,videoAdded;
        videoId = obj.value("id").toString();
        title   = obj.value("title").toString();
        thumb   = "https://i.ytimg.com/vi/"+videoId+"/mqdefault.jpg";
        duration   = obj.value("duration").toInt();
        durationStr= utils::formatSeconds(duration);
        videoAdded = i+1;

        if(addedItemsIds.lastIndexOf(videoId) == -1 && itemIdList.lastIndexOf(videoId) != -1)
        {
            addedItemArray.append(QJsonValue(obj));
            addedItemsIds.append(videoId);
            if(!title.contains("[Deleted video]",Qt::CaseInsensitive) ||
                    !title.contains("[Private video]",Qt::CaseInsensitive))
            {
                //init itemwidget & add to resultList
                VideoItem *videoItem = new VideoItem(ui->selectedItemsListWidget,this->networkManager_);
                videoItem->setObjectName("video_"+videoId);
                videoItem->hideSelector();
                videoItem->init(videoAdded,videoId,title,thumb,durationStr);
                videoItem->adjustSize();
                QListWidgetItem* item;
                item = new QListWidgetItem(ui->selectedItemsListWidget);
                ui->selectedItemsListWidget->setItemWidget(item, videoItem);
                item->setSizeHint(videoItem->sizeHint());
                ui->selectedItemsListWidget->addItem(item);
            }
        }
        if(mainWindow->quiting){
            break;
        }
        QApplication::processEvents();
    }
    ui->selectedItemsListWidget->setUpdatesEnabled(true);
}


void PlaylistDownloadOptions::resetUi()
{
    addedItemArray = QJsonArray(); //empty array

    ui->selectedItemsListWidget->clear();
    ui->selectedItemsListWidget->verticalScrollBar()->setValue(
                ui->selectedItemsListWidget->verticalScrollBar()->minimum());

    ui->videoRadioButton->setChecked(true);

    ui->mp3->setChecked(true);
    ui->mkv->setChecked(true);

    foreach (QRadioButton *btn, this->findChildren<QRadioButton*>()) {
        connect(btn,&QRadioButton::toggled,[=, this](){
            this->updateStatus();
        });
    }

    foreach(QAdvancedSlider *slider , this->findChildren<QAdvancedSlider*>())
    {
        slider->disconnect();
        slider->setRange(0,100);
        slider->setTickInterval(10);
        slider->setPageStep(10);
        slider->setSingleStep(10);
        slider->setSnappingEnabled(true);
        slider->setErrorHint(0,20);
        slider->setWarningHint(20,40);
        slider->setOptimalHint(40,70);
        slider->setBestHint(70,100);

        connect(slider,&QAdvancedSlider::valueChanged,[=, this](int value)
        {
            QString valString;
            switch (value) {
            case 0 ... 20:
                valString = "Poor";
                break;
            case 21 ... 40:
                valString = "Low";
                break;
            case 41 ... 70:
                valString = "Medium";
                break;
            case 71 ... 90:
                valString = "Good";
                break;
            case 91 ... 100:
                valString = "Best";
                break;
            default:
                valString = QString::number(value);
                break;
            }
            QLabel *label = this->findChild<QLabel*>(slider->objectName()+"Label");
            label->setText(valString);
            this->updateStatus();
        });
        slider->setValue(100);
    }

    QSettings settings;
    QString downloadPath = utils::returnExactPath(settings.value("download_path",
                          QStandardPaths::writableLocation(
                          QStandardPaths::DownloadLocation)+
                          QDir::separator()+QApplication::applicationName()).toString());
    ui->downloadLocation->setText(downloadPath);
}

PlaylistDownloadOptions::~PlaylistDownloadOptions()
{
    delete ui;
}

void PlaylistDownloadOptions::on_videoRadioButton_toggled(bool checked)
{
    if(checked)
        ui->stackedWidget->slideInWgt(ui->videoPage);
}

void PlaylistDownloadOptions::on_audioRadioButton_toggled(bool checked)
{
    if(checked)
        ui->stackedWidget->slideInWgt(ui->audioPage);
}

bool PlaylistDownloadOptions::is_pro_feature()
{
    if(ui->audioRadioButton->isChecked())
    {
        if(this->getAudioQuality() > 20){
            return true;
        }
    }else{
        if(this->getVideoQuality() > 20){
            return true;
        }
        if(this->getVideoAudioQuality() > 20){
            return true;
        }
    }
    return false;
}

QString PlaylistDownloadOptions::generateDownloadRecordFile(QString playlistId)
{
    //prepare unique filename for record file
    QString UUID = playlistId+"__S__"+QString::number(QDateTime::currentMSecsSinceEpoch());
    //directory path is taken and hard coded from DownloadWidget class's get_download_record_dir_path function
    QString filename = utils::returnPath("download_records")+UUID+".json";

    //prepare record file
    QJsonDocument recordFile;
    bool audio_only = ui->audioRadioButton->isChecked();
    QString download_type_str = QString(audio_only ? "audio":"video");

    QJsonObject recordObj;

    QJsonArray playlist_meta;
    QJsonObject playlist_info_obj;

    playlist_info_obj.insert("id",QJsonValue(this->playlist_id));
    playlist_info_obj.insert("title",QJsonValue(this->playlist_name));
    playlist_info_obj.insert("uploader",QJsonValue(this->uploader_name));

    playlist_meta.append(playlist_info_obj);

    QJsonArray download_params;
    QJsonObject download_params_obj;

    download_params_obj.insert("download_type",QJsonValue(download_type_str));
    if(audio_only){
        download_params_obj.insert("audio_quality",QJsonValue(this->getAudioQuality()));
        download_params_obj.insert("container",QJsonValue(this->getAudioContainer()));
    }else{
        download_params_obj.insert("audio_quality",QJsonValue(this->getVideoAudioQuality()));
        download_params_obj.insert("video_quality",QJsonValue(this->getVideoQuality()));
        download_params_obj.insert("container",QJsonValue(this->getVideoContainer()));
    }

    download_params_obj.insert("download_location", utils::returnExactPath(
                        ui->downloadLocation->text()+this->playlist_name+
                        "-"+QString(audio_only ? this->getAudioContainer()
                                               :this->getVideoContainer()).toUpper()));

    download_params.append(download_params_obj);

    recordObj.insert("playlist_meta",playlist_meta);

    recordObj.insert("download_params",download_params);

    recordObj.insert("items",addedItemArray);

    recordFile.setObject(recordObj);

    //save record file
    utils::saveJson(recordFile,filename);

    return filename;
}

QString PlaylistDownloadOptions::getAudioContainer()
{
    QString formatCode = "mp3";
    foreach (QRadioButton *rBtn, ui->audioFrame->findChildren<QRadioButton*>()) {
        if(rBtn->isChecked())
            formatCode = rBtn->objectName().trimmed().simplified();
    }
    return formatCode;
}

QString PlaylistDownloadOptions::getVideoContainer()
{
    QString formatCode = "mkv";
    foreach (QRadioButton *rBtn, ui->videoFrame->findChildren<QRadioButton*>()) {
        if(rBtn->isChecked())
            formatCode = rBtn->objectName().trimmed().simplified();
    }
    return formatCode;
}

void PlaylistDownloadOptions::updateStatus()
{
    bool audio_only = ui->audioRadioButton->isChecked();
    QString download_type_str = QString(audio_only ? "audio":"video").toUpper();
    QString container,format;

    if(audio_only){
        QString audio_quality = Helper::getFormatName(this->getAudioQuality());
        container= this->getAudioContainer();
        format = "Selected format: %1(%2) | %3 Audio";
        ui->statusLabel->setText(format.arg(download_type_str,container,audio_quality));
    }else{
        QString audio_quality = Helper::getFormatName(this->getVideoAudioQuality());
        QString video_quality = Helper::getFormatName(this->getVideoQuality());
        container = this->getVideoContainer();
        format = "Selected format: %1(%2) | %3 Video | %4 Audio";
        ui->statusLabel->setText(format.arg(download_type_str,container,video_quality,audio_quality));
    }
}

int PlaylistDownloadOptions::getAudioQuality()
{
    return ui->aAudioQualitySlider->value();
}

int PlaylistDownloadOptions::getVideoQuality()
{
     return ui->vVideoQualitySlider->value();
}

int PlaylistDownloadOptions::getVideoAudioQuality()
{
     return ui->vAudioQualitySlider->value();
}

void PlaylistDownloadOptions::on_addToDownload_clicked()
{
    m_account->check_pro(true);
    if(is_pro_feature())
    {
        if(m_account->evaluation_used == true && m_account->pro == false)
        {
           m_account->showPurchaseMessage("High Download quality selection");
           return;
        }
    }

    emit addToDownload(generateDownloadRecordFile(this->playlist_id));
}

void PlaylistDownloadOptions::on_changeLocation_clicked()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);

    QSettings settings;

    QString downloadPath = utils::returnExactPath(settings.value("download_path",
                          QStandardPaths::writableLocation(
                          QStandardPaths::DownloadLocation)+
                          QDir::separator()+QApplication::applicationName()).toString());

    QString path = QFileDialog::getExistingDirectory(this, tr("Choose Directory"),
                                                     downloadPath,
                                                     QFileDialog::ShowDirsOnly
                                                     | QFileDialog::DontUseNativeDialog);
    QFileInfo dir(path);
    if (dir.isDir() && dir.isWritable()) {
        ui->downloadLocation->setText(utils::returnExactPath(path));
    }else{
        QMessageBox::critical(this,tr("Error"),"The selected path cannot be used,\nPlease select a Directory that is writable.");
    }
}
