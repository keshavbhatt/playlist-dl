/*
Last Edited: Fri Mar 26 18:53:35 IST 2021
*/

#include "account.h"
#include "ui_account.h"
#include "utils.h"
#include <QTextCodec>
#include <QFile>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>

account::account(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::account)
{
    ui->setupUi(this);

    ui->offer_groupBox->hide();

    ui->email->setText(tr("In pro version only"));
    ipV6 = utils::randomIpV6();
    blinkTimer = new QTimer(this);
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    eff->setOpacity(1);
    ui->buy_external->setGraphicsEffect(eff);
    connect(blinkTimer,&QTimer::timeout,[=](){
        if(blinkCounter == 0){
            blinkTimer->stop();
            return;
        }
        blinkTimer->setInterval(1000);
        blinkCounter = blinkCounter - 1;
        QPropertyAnimation *a = new QPropertyAnimation(ui->buy_external->graphicsEffect(),"opacity");
        a->setDuration(1000);
        a->setStartValue(0);
        a->setEndValue(1);
        a->setEasingCurve(QEasingCurve::InCurve);
        a->start(QPropertyAnimation::DeleteWhenStopped);
        ui->buy_external->show();
    });

    //serverUID is unique identiifier which distinguish app on server
    serverUID = "PLDL";

    ev_time = 86400 * 30;// 30 days trial value in seconds

    // loader is the child of status
    _loader = new WaitingSpinnerWidget(ui->status,true,false);
    _loader->setRoundness(70.0);
    _loader->setMinimumTrailOpacity(15.0);
    _loader->setTrailFadePercentage(70.0);
    _loader->setNumberOfLines(10);
    _loader->setLineLength(8);
    _loader->setLineWidth(2);
    _loader->setInnerRadius(2);
    _loader->setRevolutionsPerSecond(3);
    _loader->setColor(QColor("#1e90ff"));

    setting_path =  QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    //retrive old accountId
    if(settings.value("accountId").isValid()){
        accountId = settings.value("accountId").toString();
    }

    //get account id from download location
    if(accountId.isEmpty()){
        QFile file(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/."+QApplication::applicationName()+".id");
        if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){
            qDebug()<<"unable to open file ."+QApplication::applicationName()+".id";
        }else{
            QByteArray data = file.readAll();
            accountId = QString(data).split("\n").first();
            QString val = QString(data).split("\n").last();
            settings.setValue(QApplication::applicationName()+"_emit",QByteArray::fromBase64(QByteArray(val.toUtf8())).toBase64());
            settings.setValue("accountId",accountId);
            file.close();
        }
    }
    if(accountId.isEmpty()){
        connect(this,&account::account_created,[=](){
            parent->findChild<QPushButton*>("push")->click();
        });
        createAccount();
    }else{
        ui->id->setText(accountId);
        check_purchased(accountId);
    }
    write_evaluation_val();
    check_evaluation_used();

    //init_claimOffer();
}

void account::init_claimOffer()
{
    claimOffer = new ClaimOffer(accountId,this);
    claimOffer->setWindowTitle(QApplication::applicationName()+" | Claim Offer");
    claimOffer->setWindowFlags(Qt::Dialog);
    claimOffer->setWindowModality(Qt::WindowModal);
}

void account::createAccount()
{
    accountId = utils::generateRandomId(20);
    qDebug()<<"created account"<<accountId;
    emit account_created();
    ui->id->setText(accountId);
    //save account id
    settings.setValue("accountId",accountId);
    QFile file(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/."+QApplication::applicationName()+".id");
    if(!file.open(QIODevice::ReadWrite|QIODevice::Text)){
        qDebug()<<"unable to create file."+QApplication::applicationName()+".id";
    }else{
        file.write(QByteArray(accountId.toUtf8()));
        file.close();
    }
    check_purchased(accountId);
}

//check purchased
void account::check_purchased(QString username)
{
    ui->status->clear();
    ui->restore_purchase->setText("Checking..");
    _loader->start();
    ui->restore_purchase->setEnabled(false);

    QNetworkRequest newRequest;
    newRequest.setUrl(QUrl("http://www.ktechpit.com/USS/"+serverUID+"/checkout/chkout/check.php?username="+username));

    QNetworkAccessManager *networkManager =new QNetworkAccessManager;
    QNetworkReply *reply = networkManager->get(newRequest);
    const bool connected = connect(reply,SIGNAL(finished()),this,SLOT(check_purchase_request_done()));
    qDebug()<<"ACCOUNT_UTILS"<<"Checking account for:"<<username<<connected;
}

void account::check_purchase_request_done()
{
    _loader->stop();
    qDebug()<<"ACCOUNT_UTILS"<<"check Purchase request Done";
    QNetworkReply *reply = ((QNetworkReply*)sender());
    QNetworkRequest request = reply->request();
    QString username = request.url().toString().split("username=").last();
    QByteArray ans= reply->readAll();


    QString s_data = QTextCodec::codecForMib(106)->toUnicode(ans);  //106 is textcode for UTF-8 here --- http://www.iana.org/assignments/character-sets/character-sets.xml
    if(reply->error() == QNetworkReply::NoError){
        checkedOnline = true;
        purchase_checked(username+" - "+s_data);
    }else{// we will not check if we find app is pro in previous session on any network error
        checkedOnline = false;
        if(settings.value(QApplication::applicationName()).isValid()){
           QByteArray b;
           b = settings.value(QApplication::applicationName()).toByteArray() ;
           QString s = QByteArray::fromBase64(b);
           if(s=="activated"){

           }else {
                purchase_checked(username+" - "+cleanHost(reply->errorString()));
           }
        }
        account_check_failed(cleanHost(reply->errorString()));
    }
}

QString account::cleanHost(QString hostStr)
{
    QString host = hostStr.replace("www.ktechpit.com",ipV6)
                   .remove("http://")
                   .remove("USS/");
    return host;
}

void account::account_check_failed(QString error)
{
    ui->status->setText(error);
    ui->restore_purchase->setText("Restore Purchase");
    ui->restore_purchase->setEnabled(true);
}

void account::purchase_checked(QString response)
{
    qDebug()<<"Account found="<<response;
    ui->restore_purchase->setText("Restore Purchase");
    ui->restore_purchase->setEnabled(true);
    if(response.contains("Account is active",Qt::CaseInsensitive)){
        ui->acc_type->setText("Pro");
        ui->restore_purchase->setEnabled(false);
        settings.setValue(QApplication::applicationName(),QString("activated").toUtf8().toBase64());
        emit enablePro();
        check_pro();
    }else{
        settings.setValue(QApplication::applicationName(),QString("sfjhkfngkj").toUtf8().toBase64());
        ui->acc_type->setText("Evaluation");
        ui->restore_purchase->setText("Restore Account");
        ui->restore_purchase->setEnabled(true);
        emit disablePro();
        check_pro();

        if(response.contains("plan expired on")){
            QMessageBox msgBox;
            msgBox.setText("Account Type Evaluation");
            msgBox.setIconPixmap(QPixmap(":/icons/sidebar/info.png").scaled(42,42,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            msgBox.setInformativeText(response);
            msgBox.setStandardButtons(QMessageBox::Cancel);
            msgBox.setWindowModality(Qt::ApplicationModal);
            QPushButton* account = msgBox.addButton(tr("Purchase Licence"), QMessageBox::YesRole);
            msgBox.setDefaultButton(account);
            msgBox.exec();
            if (msgBox.clickedButton()==account) {
                this->show();
                flashPurchaseButton();
            }
        }
    }
    ui->status->setText(response);
}

void account::check_pro(bool liveCheck)
{
    //check online if user started app offline or network was dead when app checked
    //account in previous attempts
    if(liveCheck && checkedOnline == false){
        check_purchased(accountId);
    }

    if(settings.value(QApplication::applicationName()).isValid()){
       QByteArray b = settings.value(QApplication::applicationName()).toByteArray() ;
       QString s = QByteArray::fromBase64(b);
       if(s=="activated"){
           pro = true;
           evaluation_used = false;
           this->setWindowTitle(QApplication::applicationName()+" Pro");
           ui->acc_type->setText("Pro");
           ui->buy_external->setEnabled(false);
           ui->restore_purchase->setEnabled(false);
           ui->email->setText("keshavnrj@gmail.com");
           ui->offer_groupBox->setEnabled(false);
           ui->offer_groupBox->hide();
       }else{
           if(settings.value(QApplication::applicationName()).isValid()){
              QByteArray b = settings.value(QApplication::applicationName()).toByteArray() ;
              QString s = QByteArray::fromBase64(b);
              if(s=="sfjhkfngkj"){
                  //evaluation_used = true;
                  check_evaluation_used();
              }
           }
           pro =false;
           this->setWindowTitle(QApplication::applicationName()+" Evaluation");
           ui->acc_type->setText("Evaluation");
           ui->buy_external->setEnabled(true);
           ui->restore_purchase->setEnabled(true);
           flashPurchaseButton();
       }
    }
    #ifdef QT_DEBUG
     //evaluation_used = true; //debug only
    #endif
}

//write once
void account::write_evaluation_val()
{
    //safe method if user deletes the conf file this will keep the ev time safe in paginator dir in .dbn file
    QFile file(setting_path+"/.dbn");
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text))
              return;
    QTextStream in(&file);
    QString time = in.readAll().trimmed();
    if(!settings.value(QApplication::applicationName()+"_emit").isValid() && time.isEmpty()){
        settings.setValue(QApplication::applicationName()+"_emit",QByteArray(QString::number(QDateTime::currentMSecsSinceEpoch()/1000).toUtf8()).toBase64());
        qDebug()<<"write ev file";
        QTextStream out(&file);
        QString val = QByteArray::fromBase64(settings.value(QApplication::applicationName()+"_emit").toByteArray());
        out<<val<<endl;
        file.close();
        //save time to .appname.id file too
        QFile file2(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/."+QApplication::applicationName()+".id");
        if(!file2.open(QIODevice::Append|QIODevice::Text)){
            return;
        }else{
            QTextStream out(&file2);
            out<<endl<<QByteArray(QString::number(QDateTime::currentMSecsSinceEpoch()/1000).toUtf8()).toBase64();
            file2.close();
        }
    }else{
        if(!time.isEmpty())
        settings.setValue(QApplication::applicationName()+"_emit",QByteArray(time.toUtf8()).toBase64());
    }
}

void account::flashPurchaseButton()
{
    blinkTimer->start(200);
    blinkCounter = 3;
}

void account::showEvent(QShowEvent *event)
{
    check_evaluation_used();
    QWidget::showEvent(event);
}

void account::check_evaluation_used()
{
    qDebug()<<"checking evaluation used";
    if(pro==false){
        QString val = QByteArray::fromBase64(settings.value(QApplication::applicationName()+"_emit").toByteArray());
        if(QDateTime::currentMSecsSinceEpoch()/1000 > val.toLongLong()+ev_time){
            evaluation_used = true;
        }else{
            evaluation_used = false;
        }
    }
    qDebug()<<"evaluatio used:"<<evaluation_used;
    if(!pro){
        if(settings.value(QApplication::applicationName()+"_emit").isValid()){
            QString val = QByteArray::fromBase64(settings.value(QApplication::applicationName()+"_emit").toByteArray());
            if(evaluation_used==true){
                ui->time_left->setText("0 days 0 hours 0 minutes");
            }else{
                ui->time_left->setText(utils::convertSectoDay(QDateTime::currentMSecsSinceEpoch()/1000  - (ev_time+val.toLongLong())).remove("-").split("minutes").first()+"minutes");
            }
        }else{
            ui->time_left->setText(utils::convertSectoDay(ev_time).remove("-").split("minutes").first()+"minutes");
        }
    }

    if(!pro && evaluation_used)
    {
        emit disablePro();
    }
}


account::~account()
{
     delete ui;
}


void account::on_buy_external_clicked()
{
    QDesktopServices::openUrl(QUrl("http://ktechpit.com/USS/"+serverUID+"/checkout/index.php?accountId="+accountId));
}

void account::on_restore_purchase_clicked()
{
    check_purchased(accountId);
}

void account::showPurchaseMessage(QString message)
{
    QString message_str = message.isEmpty() ?  "" : "("+message+")";

    QMessageBox msgBox;
    msgBox.setText("Evaluation period ended, Upgrade to Pro");
    msgBox.setIconPixmap(QPixmap(":/icons/information-line.png").scaled(42,42,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    msgBox.setInformativeText("This feature "+message_str+" is not availabe after free trail (evaluation version) of application has been used.\n\nPlease purchase a licence and support in development of application.");
    msgBox.setStandardButtons(QMessageBox::Cancel);
    msgBox.setWindowModality(Qt::ApplicationModal);
    QPushButton* account = msgBox.addButton(tr("Purchase Licence"), QMessageBox::YesRole);
    msgBox.setDefaultButton(account);
    msgBox.exec();
    if (msgBox.clickedButton()==account) {
        emit showAccountWidget();
    }else{
        msgBox.close();
    }
}

void account::on_offer_claim_ins_clicked()
{
    if(claimOffer == nullptr)
        return;

    if(!claimOffer->isVisible())
        claimOffer->show();
    else
        claimOffer->setWindowState(Qt::WindowActive);
}
