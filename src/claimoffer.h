#ifndef CLAIMOFFER_H
#define CLAIMOFFER_H

#include <QWidget>
#include <QSettings>

namespace Ui {
class ClaimOffer;
}

class ClaimOffer : public QWidget
{
    Q_OBJECT

public:
    explicit ClaimOffer(QString accountId = "",QWidget *parent = nullptr);
    ~ClaimOffer();

protected slots:
    void resizeEvent(QResizeEvent *event);
private slots:
    void on_claimButton_clicked();

    void on_video_id_textChanged(const QString &arg1);

    void updateOverlay(QString tense="");
private:
    Ui::ClaimOffer *ui;
    QSettings settings;
    QWidget *overlay;
};

#endif // CLAIMOFFER_H
