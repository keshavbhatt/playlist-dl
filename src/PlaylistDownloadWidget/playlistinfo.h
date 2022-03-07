#ifndef PLAYLISTINFO_H
#define PLAYLISTINFO_H

#include <QWidget>

namespace Ui {
class PLaylistInfo;
}

class PLaylistInfo : public QWidget
{
    Q_OBJECT

public:
    explicit PLaylistInfo(QWidget *parent = nullptr, QString download_record_filename = "");
    ~PLaylistInfo();

private slots:
    void load();
private:
    Ui::PLaylistInfo *ui;
    QString download_record_filename;
};

#endif // PLAYLISTINFO_H
