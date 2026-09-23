#include "playlistinfo.h"
#include "ui_playlistinfo.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QProcess>
#include <QMessageBox>

#include "utils.h"
#include "helper.h"

PLaylistInfo::PLaylistInfo(QWidget *parent, QString download_record_filename) :
    QWidget(parent),
    ui(new Ui::PLaylistInfo)
{
    ui->setupUi(this);
    if(download_record_filename.isEmpty()){
        this->close();
    }else{
        this->download_record_filename = download_record_filename;
    }

    foreach (QLabel *label, ui->mainWidget->findChildren<QLabel*>()) {
        label->setTextInteractionFlags(Qt::TextSelectableByMouse
                                       | Qt::LinksAccessibleByMouse
                                       | Qt::LinksAccessibleByKeyboard);
        connect(label,&QLabel::linkActivated,[=, this](QString link)
        {
            QFileInfo info(link);
            if(info.exists() == false){
                QMessageBox::information(this,QApplication::applicationName()+" | Error","Unale to locate directory.");
            }
            QProcess xdg_open;
            xdg_open.setProgram("xdg-open");
            qInfo()<<"xdg-open"<<link;
            xdg_open.setArguments(QStringList()<<link);

            xdg_open.start();
            if(xdg_open.waitForFinished() == false)
            {
                if( QDesktopServices::openUrl(QUrl("file://"+link))  == false)
                {
                    QDesktopServices::openUrl(QUrl(link.split("link://").last()));
                }
            }
        });
    }
    load();
}

void PLaylistInfo::load()
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

    auto valObj = metaArray.first().toObject();
    QString playlist_id = valObj.value("id").toString();
    QString playlist_name = valObj.value("title").toString();
    QString playlist_uploader = valObj.value("uploader").toString();


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


    ui->id->setText(playlist_id);
    ui->title->setText(playlist_name);
    ui->downlaod_type->setText(download_type.toUpper());
    ui->item_count->setText(QString::number(itemsArray.count()));
    ui->container->setText(container.toUpper());
    ui->added_on->setText(addedStr);
    ui->uploader_id->setText(playlist_uploader);
    ui->download_location->setText("<a href=\""+download_location+"\">"+download_location+"</a>");

    if(quality.count() == 2){
        ui->audio_quality->setText(Helper::getFormatName(quality.at(0)) + " Audio");
        ui->video_quality->setText(Helper::getFormatName(quality.at(1)) + " Video");
    }else if(quality.count() == 1){
        ui->audio_quality->setText(Helper::getFormatName(quality.at(0)) + " Audio");
        ui->video_quality->setText("");
    }
}

PLaylistInfo::~PLaylistInfo()
{
    delete ui;
}
