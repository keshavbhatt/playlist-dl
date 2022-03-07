#include "spinner.h"
#include "ui_spinner.h"

//#include <QDebug>

Spinner::Spinner(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Spinner)
{
    ui->setupUi(this);

    //init spinner
    _loader = new WaitingSpinnerWidget(ui->widget,true,false);
    _loader->setRoundness(70.0);
    _loader->setMinimumTrailOpacity(15.0);
    _loader->setTrailFadePercentage(70.0);
    _loader->setNumberOfLines(8);
    _loader->setLineLength(8);
    _loader->setLineWidth(2);
    _loader->setInnerRadius(2);
    _loader->setRevolutionsPerSecond(3);
    _loader->setColor(QColor("#1e90ff"));

}

void Spinner::setSpinnerSize(QSize size)
{
    QSize nuSize = size - QSize(4,4);
    //qDebug()<<size<<nuSize;
    ui->widget->setFixedSize(nuSize);
    _loader->setLineLength(nuSize.height()/6);
}

void Spinner::stop()
{
    ui->widget->hide();
    this->_loader->stop();
}

void Spinner::start()
{
    ui->widget->show();
    this->_loader->start();
}

Spinner::~Spinner()
{
    delete ui;
}
