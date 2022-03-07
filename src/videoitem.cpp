#include "videoitem.h"
#include "ui_videoitem.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "mainwindow.h"

VideoItem::VideoItem(QWidget *parent, QNetworkAccessManager *manager) :
    QWidget(parent),
    ui(new Ui::VideoItem)
{
    ui->setupUi(this);

    this->networkManager_ = manager;

    QObject *mainWindowObject = utils::getMainWindow(this);
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(mainWindowObject);

    connect(this,&VideoItem::playVideo,mainWindow,&MainWindow::playVideo);

    //animate();
}

void VideoItem::animate()
{
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(600);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::Linear);
    a->start(QPropertyAnimation::DeleteWhenStopped);
    connect(a,&QPropertyAnimation::finished,[this,eff](){
       eff->deleteLater();
    });
}

void VideoItem::init(int pos, QString videoId, QString title, QString thumb,QString duration)
{
    this->videoId = videoId;
    ui->pos->setText("#"+QString::number(pos));
    ui->title->setText(title);
    ui->thumbnail->setPixmap(QPixmap(":/icons/others/wall_placeholder_180.jpg").scaled(ui->thumbnail->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
    ui->thumbnail->init(this->networkManager_,thumb,":/icons/others/wall_placeholder_180.jpg");
    ui->duration->setText(duration);
}

VideoItem::~VideoItem()
{
    delete ui;
}

QString VideoItem::getVideoId()
{
    return videoId;
}

void VideoItem::hideSelector()
{
    ui->checkBox->hide();
    ui->verticalLayout_2->setContentsMargins(0,0,0,0);
}

void VideoItem::on_checkBox_toggled(bool checked)
{
    Q_UNUSED(checked);
    emit itemCheckedChanged();
}

void VideoItem::on_play_clicked()
{
    emit playVideo(this->videoId);
}
