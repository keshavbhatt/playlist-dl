#include "downloaditem.h"
#include "ui_downloaditem.h"

#include <QFileInfo>
#include "utils.h"

DownloadItem::DownloadItem(QWidget *parent, QString uid) :
    QWidget(parent),
    ui(new Ui::DownloadItem)
{
    ui->setupUi(this);

    this->uuid = uid;
    this->videoId = QString(uid).split("__V__").last();
    this->parent_playlistId = QString(uid).split("__V__").first();

    QString path = QString("progress")+QDir::separator()+getParentPlaylistId();
    QString progresFilePath = utils::returnPath(path);

    QString progressFileName = progresFilePath+this->getVideoId();
    settings =  new QSettings(progressFileName,QSettings::NativeFormat,this);

    statusColor.insert("error","background-color: rgb(204, 0, 0);");
    statusColor.insert("running","background-color: rgb(0, 204, 0);");
    statusColor.insert("paused","background-color: rgb(239, 167, 75);");
    statusColor.insert("idle","background-color: rgba(239, 167, 75, 0);");
    statusColor.insert("finished","background-color: rgb(110, 179, 228);");
    statusColor.insert("queued","background-color: rgb(245, 241, 104);");

    init_progressBar();
    loadProgress();
}

void DownloadItem::setStatus(DownloadProcess::Status status)
{
    ui->statisInfoLabel->setText(settings->value("status").toString());
    switch (status) {
    case DownloadProcess::Status::Failed:
        ui->statusLabel->setStyleSheet(statusColor.value("error"));
        settings->setValue("statusStyle",ui->statusLabel->styleSheet());
        break;
    case DownloadProcess::Status::Finished:
        ui->statusLabel->setStyleSheet(statusColor.value("finished"));
        settings->setValue("statusStyle",ui->statusLabel->styleSheet());
        break;
    case DownloadProcess::Status::Queued:
        ui->statusLabel->setStyleSheet(statusColor.value("queued"));
        //settings->setValue("statusStyle",ui->statusLabel->styleSheet());
        break;
    default:
        break;
    }
}

void DownloadItem::setState(DownloadProcess::ProcessState state)
{
    switch (state) {
    case DownloadProcess::Running:
        ui->statisInfoLabel->setText("running"); // we should update progress here
        ui->statusLabel->setStyleSheet(statusColor.value("running"));
        break;
    case DownloadProcess::NotRunning:
        ui->statisInfoLabel->setText("idle");
        ui->statusLabel->setStyleSheet(statusColor.value("idle"));
        break;
    case DownloadProcess::Starting:
        ui->statisInfoLabel->setText("starting");
        ui->statusLabel->setStyleSheet(statusColor.value("running"));
        break;
    default:
        break;
    }
}



/**
 * @brief DownloadItem::init initialize the UI if item
 * @param id videoId
 * @param title videoTitle
 * @param thumb thumbnailUrl
 * @param durationStr durationStr
 */
void DownloadItem::init(QString id, QString title, QString thumb, QString durationStr)
{
    this->videoId     = id;
    this->videoTitle  = title;
    this->durationStr = durationStr;
    this->thumbUrl    = thumb;

    ui->title->setText(title);
    ui->duration->setText(durationStr);
}


/**
 * @brief DownloadItem::getVideoId
 * @return videoId : template videoId;
 */
QString DownloadItem::getVideoId()
{
    return videoId;
}

QString DownloadItem::getVideoTitle()
{
    return videoTitle;
}

QString DownloadItem::getDurationStr()
{
    return durationStr;
}

QString DownloadItem::getThumbUrl()
{
    return thumbUrl;
}

/**
 * @brief DownloadItem::getUUID
 * @return complete UUID : template playlistId__S__timeadded__V__videoId;
 */
QString DownloadItem::getUUID()
{
    return uuid;
}

/**
 * @brief DownloadItem::getParentPlaylistId
 * @return parent playlistID : template playlistId__S__timeadded;
 */
QString DownloadItem::getParentPlaylistId()
{
    return parent_playlistId;
}

void DownloadItem::init_progressBar()
{
    circularProgressBar = new CircularProgessBar(this);
    circularProgressBar->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);
    circularProgressBar->setArcPenBrush(QBrush("#5DB8DE"));
    circularProgressBar->setCircleBrush(QBrush(QColor(93,184,222,120)),QBrush(QColor(93,184,222,80)));
    QFont f;
    f.setFamily(f.defaultFamily());
    f.setPixelSize(12);
    f.setBold(true);
    circularProgressBar->setTextProperty(QColor(255,255,255,255),f);
    int itemHeight = this->sizeHint().height()-9;
    circularProgressBar->setMinimumSize(itemHeight,itemHeight);
    ui->progressBarLayout->addWidget(circularProgressBar);

    //read previous settings
    double progress = settings->value("progress",0.0).toDouble();
    QString text = QString::number(progress,'f',1);
    circularProgressBar->setText(text);
    circularProgressBar->setProgressValue(progress);
}

void DownloadItem::loadProgress()
{
    if(settings->value("hasProgressMap",false).toBool() == false &&
            (settings->value("status","idle").toString() != "finished"
             && settings->value("status","idle").toString() != "error")){
        ui->statusLabel->setStyleSheet(statusColor.value("idle"));
        return;
    }

    circularProgressBar->setText(settings->value("progress_exact","0.0").toString());
    circularProgressBar->setProgressValue(settings->value("progress_exact","0.0").toDouble());
    if(hasDownloadProcess() && dp->isRunning()){
        QString statusString = "Downloaded: %1 of %2,  Speed: %3";
        ui->statisInfoLabel->setText(statusString.arg(settings->value("downloaded").toString(),
                                                      settings->value("size").toString(),
                                                      settings->value("speed").toString()));
        ui->statusLabel->setStyleSheet(statusColor.value("running"));
        qDebug()<<"HAS PROCESS"<<"IS RUNNING";
    }else{
        QString status = settings->value("status","idle").toString();
        ui->statisInfoLabel->setText(status);
        ui->statusLabel->setStyleSheet(statusColor.value(status));
        qDebug()<<"STATUS"<<status;
    }
}

void DownloadItem::setProgress(QMap<QString,QString> progressMap)
{
    settings->setValue("hasProgressMap",true);
    circularProgressBar->setText(progressMap.value("progress_exact"));
    circularProgressBar->setProgressValue(progressMap.value("progress_exact").toDouble());
    QString statusString = "Downloaded: %1 of %2,  Speed: %3";
    ui->statisInfoLabel->setText(statusString.arg(progressMap.value("downloaded"),
                                                  progressMap.value("size"),
                                                  progressMap.value("speed")));
    ui->statusLabel->setStyleSheet(statusColor.value("running"));
}

bool DownloadItem::hasDownloadProcess()
{
    return dp != nullptr;
}

void DownloadItem::setProcess(DownloadProcess *process)
{
    dp = process;
    dp->disconnect(this);

    //do not use lamda functors to connect these signals since we will need context object to disconnect them
    //https://stackoverflow.com/questions/14828678/disconnecting-lambda-functions-in-qt5
    connect(dp,SIGNAL(stateChanged(DownloadProcess::ProcessState)),this,SLOT(setState(DownloadProcess::ProcessState)));
    connect(dp,SIGNAL(downloadProgressChanged(QMap<QString,QString>)),this,SLOT(setProgress(QMap<QString,QString>)));
    connect(dp,SIGNAL(statusChanged(DownloadProcess::Status)),this,SLOT(setStatus(DownloadProcess::Status)));

}

DownloadItem::~DownloadItem()
{
    if(dp != nullptr)
    {
        //dp->disconnect(this);
        dp = nullptr;
        delete dp;
    }
    settings->deleteLater();
    delete ui;
}
