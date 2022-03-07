#ifndef HELPER_H
#define HELPER_H

#include <QObject>

class Helper
{
public:
    Helper();
public slots:
    static QString getFormatName(int value);
    static bool isDownloaded(QString videoId, QString UUID);
};

#endif // HELPER_H
