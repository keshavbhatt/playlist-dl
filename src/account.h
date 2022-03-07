#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QObject>
#include <QWidget>
#include <QSettings>
#include <QtNetwork>
#include "waitingspinnerwidget.h"
#include "claimoffer.h"


namespace Ui {
class account;
}

class account : public QWidget
{
    Q_OBJECT

public:
    explicit account(QWidget *parent = nullptr);
    ~account();
    QString accountId;
    QSettings settings;
    QString setting_path;

    bool pro = false;
    bool evaluation_used = false;

signals:
    void account_created();
    void showAccountWidget();
    void disablePro();
    void enablePro();

private:
     Ui::account *ui;
     int ev_time;
     QString serverUID ;
     WaitingSpinnerWidget *_loader = nullptr;
     ClaimOffer *claimOffer = nullptr;
     QString ipV6;
     QTimer *blinkTimer;
     int blinkCounter = 0;
     bool checkedOnline = false;

public slots:
     void check_evaluation_used();
     void showPurchaseMessage(QString message = "");
     void flashPurchaseButton();
     void check_pro(bool liveCheck=false);
protected slots:
     void showEvent(QShowEvent *event);
private slots:
     void createAccount();
     void check_purchase_request_done();
     void check_purchased(QString username);
     void purchase_checked(QString response);
     void account_check_failed(QString error);
     void on_buy_external_clicked();
     void on_restore_purchase_clicked();
     void write_evaluation_val();

     QString cleanHost(QString hostStr);
     void on_offer_claim_ins_clicked();

     void init_claimOffer();


};

#endif // ACCOUNT_H
