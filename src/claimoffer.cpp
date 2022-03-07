#include "claimoffer.h"
#include "ui_claimoffer.h"
#include "request.h"

#include <QMessageBox>
#include <QProgressBar>

ClaimOffer::ClaimOffer(QString accountId, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClaimOffer)
{
    ui->setupUi(this);
    ui->account_id->setText(accountId);
    ui->claimButton->setEnabled(false);

    if(settings.value("submitted_video_id").isValid()){
        ui->video_id->setText(settings.value("submitted_video_id").toString());
    }
    if(settings.value("submitted_email_id").isValid()){
        ui->email_id->setText(settings.value("submitted_email_id").toString());
    }



    overlay = new QWidget(this);
    overlay->setObjectName("overlay_widget");
    overlay->setStyleSheet("background-color:rgba(0,0,0,0.5);color:silver");
    QLabel *label              = new QLabel(overlay);
    QProgressBar *progressbar  = new QProgressBar(overlay);
    QPushButton *cancel_button = new QPushButton("Cancel");
    connect(cancel_button,&QPushButton::clicked,[=](){
       auto *req = this->findChild<Request*>();
       if(req != nullptr){
           req->blockSignals(true);
           req->deleteLater();
           overlay->hide();
       }
    });
    QVBoxLayout *overlay_layout = new QVBoxLayout(overlay);
    label->setText("Please wait...");
    label->setAlignment(Qt::AlignCenter);
    label->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    overlay_layout->addWidget(label);
    overlay_layout->addWidget(progressbar);
    overlay_layout->addWidget(cancel_button);
    overlay->setLayout(overlay_layout);
    overlay->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    overlay->setGeometry(this->geometry());
    overlay->hide();


    if(settings.value("claim_submitted").isValid()
            && settings.value("claim_submitted").toBool() == true){
        ui->video_id->setEnabled(false);
        ui->email_id->setEnabled(false);
        ui->claimButton->setEnabled(false);
        updateOverlay("already");
    }
}

void ClaimOffer::updateOverlay(QString tense)
{
    QWidget *overlay_widget = this->findChild<QWidget*>("overlay_widget");
    foreach (QWidget* wid, overlay_widget->findChildren<QWidget*>()) {
        if(wid->inherits("QLabel")){
            static_cast<QLabel*>(wid)->setText("Claim "+tense+" submitted for video id: "
                                               +settings.value("submitted_video_id").toString());
        }else{
            wid->hide();
        }
    }
    overlay_widget->show();
}

void ClaimOffer::resizeEvent(QResizeEvent *event)
{
    QWidget *overlay_widget = this->findChild<QWidget*>("overlay_widget");
    if(overlay_widget){
        overlay_widget->setGeometry(this->rect());
    }
    QWidget::resizeEvent(event);
}

ClaimOffer::~ClaimOffer()
{
    delete ui;
}

void ClaimOffer::on_claimButton_clicked()
{
    //all the below operations should happen on server.

    //get video description check if it contains account ID.
    //check the accounts db for previous claims on this video ID and account ID.

    //based on server response give user feedback.
    auto *progressbar = overlay->findChild<QProgressBar*>();

    Request *req = new Request(this);
    connect(req,&Request::requestStarted,[=](){
        overlay->show();
        if(progressbar!=nullptr)
            progressbar->setRange(0,0);
    });
    connect(req,&Request::requestFinished,[=](QString rep){
        overlay->hide();
        if(progressbar!=nullptr)
            progressbar->setRange(0,100);

        if(rep.contains("Done,",Qt::CaseSensitive)){
            settings.setValue("claim_submitted",true);
            settings.setValue("submitted_video_id",ui->video_id->text().trimmed());
            settings.setValue("submitted_email_id",ui->email_id->text().trimmed());
            updateOverlay();
        }
        QMessageBox::information(this,this->windowTitle()+"- Information",
                                 rep,QMessageBox::Ok);
    });
    connect(req,&Request::downloadError,[=](QString errorString){
        overlay->hide();
        if(progressbar!=nullptr)
            progressbar->setRange(0,100);
        QMessageBox::critical(this,this->windowTitle()+"- Error",
                                 errorString,QMessageBox::Ok);
    });

    QUrl url = QUrl("http://ktechpit.com/USS/PLDL/offer/index.php");
    QUrlQuery query;
    query.addQueryItem("acc_id",ui->account_id->text().trimmed());
    query.addQueryItem("email_id",ui->email_id->text().trimmed());
    query.addQueryItem("v_id",ui->video_id->text().trimmed());
    url.setQuery(query);
    req->get(url);
}

void ClaimOffer::on_video_id_textChanged(const QString &arg1)
{
    ui->claimButton->setEnabled(arg1.length()>=11);
}
