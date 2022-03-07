#include "helper.h"
#include <QSettings>
#include <QDir>
#include "utils.h"

Helper::Helper()
{

}

QString Helper::getFormatName(int value)
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
    return valString;
}

bool Helper::isDownloaded(QString videoId,QString UUID)
{
    QString path = QString("progress")+QDir::separator()+UUID;
    QString progresFilePath = utils::returnPath(path);

    QString progressFileName = progresFilePath+videoId;
    QSettings videoRecord(progressFileName,QSettings::NativeFormat);

    //check via status
    QString status = videoRecord.value("status","").toString();
    return status.contains("finished",Qt::CaseInsensitive);
}
