#include "playlistentryitem.h"
#include "ui_playlistentryitem.h"

#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <helper.h>
#include <QDesktopServices>
#include <QMessageBox>

PlaylistEntryItem::PlaylistEntryItem(QWidget *parent, QNetworkAccessManager *manager) :
    QWidget(parent),
    ui(new Ui::PlaylistEntryItem)
{
    ui->setupUi(this);

    overlay_widget = new QWidget(this);
    overlay_widget->setObjectName("overlay");
    overlay_ui.setupUi(overlay_widget);
    overlay_widget->setMinimumWidth(66);
    overlay_widget->setStyleSheet("QWidget#overlay{background-color: "
                                  "qlineargradient(spread:reflect, x1:0,"
                                  " y1:1, x2:1, y2:1,"
                                  " stop:0        rgba(0, 0, 0, 250),"
                                  " stop:0.298358 rgba(0, 0, 0, 210),"
                                  " stop:0.59194  rgba(0, 0, 0, 170),"
                                  " stop:0.795522 rgba(0, 0, 0, 130)"
                                  " stop:0.895522 rgba(0, 0, 0, 90),"
                                  " stop:0.995522 rgba(0, 0, 0, 10)"
                                  ");} QLabel{color:white;}");
    overlay_widget->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    overlay_widget->move(QPoint(ui->statusLabel->width(),ui->statusLabel->y()));
    overlay_widget->show();

    this->networkManager_ = manager;

    statusColor.insert("error","background-color: rgb(204, 0, 0);");
    statusColor.insert("running","background-color: rgb(0, 204, 0);");
    statusColor.insert("paused","background-color: rgb(239, 167, 75);");
    statusColor.insert("idle","background-color: rgba(239, 167, 75, 0);");
    statusColor.insert("finished","background-color: rgb(110, 179, 228);");
    statusColor.insert("queued","background-color: rgb(245, 241, 104);");

}

void PlaylistEntryItem::init_progressFile()
{
    QString path = QString("progress")+QDir::separator()+this->getPlaylistUUID();
    QString progresFilePath = utils::returnPath(path);

    QString progressFileName = progresFilePath+"index";
    //qDebug()<<"Main Playlistitem progress file name"<<progressFileName;
    settings =  new QSettings(progressFileName,QSettings::NativeFormat,this);
}

void PlaylistEntryItem::resizeEvent(QResizeEvent *event)
{
    overlay_widget->resize(overlay_widget->width(),ui->statusLabel->height());
    QWidget::resizeEvent(event);
}

void PlaylistEntryItem::moveEvent(QMoveEvent *event)
{
    overlay_widget->move(QPoint(ui->statusLabel->width(),ui->statusLabel->y()));
    QWidget::moveEvent(event);
}

void PlaylistEntryItem::setStatus(PlaylistEntryItem::Status status)
{
    switch (status) {
    case Status::Error:
        ui->statusLabel->setStyleSheet(statusColor.value("error"));
        break;
    case Status::Running:
        ui->statusLabel->setStyleSheet(statusColor.value("running"));
        break;
    case Status::NotRunning:
        settings->setValue("queued", 0);
        ui->statusLabel->setStyleSheet(statusColor.value("idle"));
        break;
    case Status::Finished:
        settings->setValue("queued", 0);
        ui->statusLabel->setStyleSheet(statusColor.value("finished"));
        break;
    case Status::Queued:
        ui->statusLabel->setStyleSheet(statusColor.value("queued"));
        break;
    default:
        settings->setValue("queued", 0);
        ui->statusLabel->setStyleSheet(statusColor.value("idle"));
        break;
    }
}

void PlaylistEntryItem::animate()
{
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(800);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::Linear);
    connect(a,&QPropertyAnimation::finished,[this,eff](){
       eff->deleteLater();
    });
    a->start(QPropertyAnimation::DeleteWhenStopped);
}

void PlaylistEntryItem::init(QString download_record_filename, QString id, QString title, QString playlistThumbnail, QString author, QString authorId,
                        int videoCount, QString videoMetaStr, QList<int> quality, QString container, QString download_type, QString download_location)
{
    Q_UNUSED(authorId);
    itemsCount = videoCount;
    playlistId = id;
    downloadLocation = download_location;
    this->download_record_filename = download_record_filename;

    if(playlistThumbnail.isEmpty()){
        playlistThumbnail = "https://i.ytimg.com/vi/"+id+"/mqdefault.jpg";
    }

    //ui->thumbnail->hide();
    ui->thumbnail->setPixmap(QPixmap(":/icons/others/wall_placeholder_180.jpg").scaled(ui->thumbnail->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
    ui->thumbnail->init(this->networkManager_,playlistThumbnail,":/icons/others/wall_placeholder_180.jpg");

    ui->title->setText(title);
    ui->by->setText(author);
    //ui->videoCount->setText(QString::number(videoCount)+QString( videoCount == 1 ? tr(" item"):tr(" items")));

    if(download_type == "audio")
    {
        overlay_ui.typeLabel->setPixmap(QPixmap(":/icons/white/music-2-fill.png"));
    }else{
        overlay_ui.typeLabel->setPixmap(QPixmap(":/icons/white/film-fill.png"));
    }

    overlay_ui.containerLabel->setText(container.toUpper());
    overlay_ui.countLabel->setText("C: "+QString::number(videoCount));

    if(quality.count() == 2){
        overlay_ui.aq->setText("A:"+Helper::getFormatName(quality.at(0)));
        overlay_ui.vq->setText("V:"+Helper::getFormatName(quality.at(1)));
    }else{
        overlay_ui.vq->hide();
        overlay_ui.aq->setText("A:"+Helper::getFormatName(quality.at(0)));
    }

    ui->videos->setText(videoMetaStr);

    overlay_widget->resize(overlay_widget->minimumSizeHint().width(),ui->statusLabel->height());

    init_progressFile();

    //reset queued items count
    settings->setValue("queued", 0);

    //reset failed items count
    settings->setValue("failed", 0);

    updateStatusText();
}

QString PlaylistEntryItem::getPlaylistDownloadLocation()
{
    return this->downloadLocation;
}


QString PlaylistEntryItem::getPlaylistId()
{
    return this->playlistId;
}

QString PlaylistEntryItem::getPlaylistUUID()
{
    QFileInfo f_info(this->getPlaylistRecordFileName());
    QString UUID = f_info.baseName(); // template : playlistId__S__timeadded;
    return UUID;
}

int PlaylistEntryItem::getPlaylistItemsCount()
{
    return this->itemsCount;
}

QString PlaylistEntryItem::getPlaylistRecordFileName()
{
    return this->download_record_filename;
}


void PlaylistEntryItem::on_menuButton_clicked()
{

    QMenu menu;
    QAction *viewPlaylistAction = new QAction(QIcon(":/icons/folder-open-line.png"), tr("View Playlist"), &menu);
    QAction *showPlaylistDetailsAction = new QAction(QIcon(":/icons/bookmark-3-line.png"), tr("Show Playlist Info"), &menu);
    QAction *openDownloadFolderAction = new QAction(QIcon(":/icons/folder-open-line.png"), tr("Open Download Location"), &menu);

    connect(openDownloadFolderAction,&QAction::triggered,[=]
    {
        QString link = getPlaylistDownloadLocation();
        QFileInfo info(link);
        if(info.exists() == false){
            QMessageBox::information(this,QApplication::applicationName()+" | Error","Unale to locate directory.");
        }

        QProcess xdg_open;
        xdg_open.setProgram("xdg-open");
        xdg_open.setArguments(QStringList()<<link);
        xdg_open.start();
        if(xdg_open.waitForFinished() == false)
        {
            if( QDesktopServices::openUrl(QUrl("file://"+link)) == false){
                QDesktopServices::openUrl(QUrl(link));
            }
        }
    });

    connect(viewPlaylistAction,&QAction::triggered,[=](){
        emit viewPlaylist(this->playlistId);
    });

    connect(showPlaylistDetailsAction,&QAction::triggered,[=](){
        emit viewPlaylistInfo();
    });

    menu.addAction(viewPlaylistAction);
    menu.addAction(showPlaylistDetailsAction);
    menu.addSeparator();
    menu.addAction(openDownloadFolderAction);

    emit selectItem(this->pos());

    menu.exec(ui->menuButton->mapToGlobal(QPoint(0,ui->menuButton->width())));

    viewPlaylistAction->deleteLater();
    showPlaylistDetailsAction->deleteLater();
}

PlaylistEntryItem::~PlaylistEntryItem()
{
    settings->deleteLater();
    delete ui;
}

void PlaylistEntryItem::setStatusText(QString str)
{
    ui->status->setText(str);
}


void PlaylistEntryItem::updateStatusText()
{
    int finished    = settings->value("finished", 0).toInt();
    int failed      = settings->value("failed", 0).toInt();
    int queued      = settings->value("queued", 0).toInt();
    int total       = getPlaylistItemsCount();

    QString status = "Downloaded %1 of %2      Failed: %3      Queued: %4";
    setStatusText(status.arg(
                        QString::number(finished),
                        QString::number(total),
                        QString::number(failed),
                        QString::number(queued)
                      ));
}

void PlaylistEntryItem::appendFinished()
{
    int finished = settings->value("finished", 0).toInt();
    settings->setValue("finished", finished + 1);

    int queued = settings->value("queued", 0).toInt();
    settings->setValue("queued", queued - 1);

    updateStatusText();
}

void PlaylistEntryItem::appendFailed()
{
    int failed = settings->value("failed", 0).toInt();
    settings->setValue("failed", failed + 1);

    int queued = settings->value("queued", 0).toInt();
    settings->setValue("queued", queued - 1);

    updateStatusText();
}

void PlaylistEntryItem::appendQueued()
{
    int queued = settings->value("queued", 0).toInt();
    settings->setValue("queued", queued + 1);

    updateStatusText();
}


void PlaylistEntryItem::resetFailed()
{
    settings->setValue("failed", 0);
    updateStatusText();
}

void PlaylistEntryItem::resetQueued()
{
    settings->setValue("queued", 0);
    updateStatusText();
}



