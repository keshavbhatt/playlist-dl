#ifndef UTILS_H
#define UTILS_H

#include <QObject>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QTextDocument>
#include <QUuid>
#include <QJsonDocument>
#include <regex>

class utils : public QObject
{
    Q_OBJECT

public:
    utils(QObject* parent=0);
    virtual ~utils();
public slots:
    static QString refreshCacheSize(const QString cache_dir);
    static bool delete_cache(const QString cache_dir);
    static QString toCamelCase(const QString &s);
    static QString generateRandomId(int length);
    static QString genRand(int length);
    static QString convertSectoDay(qint64 secs);
    static QString returnPath(QString pathname);
    static QString EncodeXML ( const QString& encodeMe );
    static QString DecodeXML ( const QString& decodeMe );
    static QString htmlToPlainText(QString str);
    static QString GetEnvironmentVar(const QString &variable_name);
    static float RoundToOneDecimal(float number);
    void DisplayExceptionErrorDialog(const QString &error_info);
    static QString appDebugInfo();

    static QJsonDocument loadJson(QString fileName);
    static void saveJson(QJsonDocument document, QString fileName);
    static QString formatSeconds(int seconds);
    static QObject *getMainWindow(QObject *self);
    static bool is_number(std::string token);
    static QString returnExactPath(QString pathname);
    static QString randomIpV6();
private slots:
    //use refreshCacheSize
    static quint64 dir_size(const QString &directory);



};

#endif // UTILS_H
